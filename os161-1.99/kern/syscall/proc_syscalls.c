#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <mips/trapframe.h>
#include <synch.h>
#include <vfs.h>
#include <vm.h>
#include <kern/fcntl.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */


static void sys_exit_helper (struct proc *p) {
  
  long int size_array = array_num(p->numberofChildProcess) - 1;
  int counter = 0;
  int true_counter = 0;
  int false_counter = 0;
  int temporary = 10;
  int switch_case = 1;

  while (true) {

      if (size_array < 0) {
        break;
      }

      struct proc *temp = array_get(p->numberofChildProcess, size_array);
      temp->parentProcessPointer = NULL;

      switch(temp->process_terminated) {
        switch_case = true_counter * temporary;
        case true:
          true_counter = true_counter + 1;
          // kprintf("true_counter = %d",  true_counter);
          proc_destroy(temp);
          break;
        case false:
          false_counter = false_counter + 1;
          // What to do here? Add later
          break;
      } 
      switch_case = switch_case + 1;
      array_remove(p->numberofChildProcess, size_array);
      counter = counter + 1;
      // kprintf("counter = %d",  counter);
      size_array = size_array - 1;
  }

}

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  // (void)exitcode;

  #if OPT_A2

  if (curproc->parentProcessPointer != NULL) {


    lock_acquire(curproc->lock_child);
    curproc->process_terminated = true;
    sys_exit_helper(curproc);
    lock_release(curproc->lock_child);
    curproc->exit_code = exitcode;
    cv_signal(curproc->cv_child, curproc->parentProcessPointer->lock_child);

  }

#endif


  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */

#if OPT_A2
  if(!p->parentProcessPointer) {
    proc_destroy(p);
  }
#endif

    
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
 
#if OPT_A2
    *retval = curproc->process_id;  
#endif
  //*retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  #if OPT_A2


  lock_acquire(curproc->lock_child);
   
  unsigned int size_array = array_num(curproc->numberofChildProcess);

  for (unsigned int i = 0; i <= size_array - 1; i++) {
    struct proc *childp = array_get(curproc->numberofChildProcess, i);
    
    if (childp->process_id == pid) {
      if (childp->process_terminated == false) {
        while (true) {
          if (childp->process_terminated) {
            break;
          }
          cv_wait(childp->cv_child, curproc->lock_child);        
        }
        exitstatus = _MKWAIT_EXIT(childp->exit_code);
      } else {
        exitstatus = _MKWAIT_EXIT(childp->exit_code);
      }
    }
    
  }

  lock_release(curproc->lock_child);

#endif

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  // exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2

int sys_fork(struct trapframe *tf, pid_t *retval) {

  struct proc *childProcess = proc_create_runprogram("childProcess"); // Add name here

  if (childProcess == NULL) {
    // return error code - enomem no memory left
    return ENOMEM;
  }

  struct addrspace *childAddrspace;
  struct addrspace *currentAddrspace = curproc_getas();

  int valueAddSpace = as_copy(currentAddrspace, &childAddrspace);

  if (valueAddSpace != 0) {
    // error code here
    return ENOMEM;
  }

  // Now attach this address space to the this child
  lock_acquire(curproc->lock_child);
	childProcess->p_addrspace = childAddrspace;
	
  // Assign exitcode
  //childProcess->code_exit = -1;

  // Assigning parent process pointer
  childProcess->parentProcessPointer = curproc;
  
  // Adding this child process to the parent's process child  
  int is_error = array_add(curproc->numberofChildProcess, childProcess, NULL);
  
  if(is_error) {
    panic("Unable to copy child");
  }

  lock_release(curproc->lock_child);

  struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  memcpy(child_tf, tf, sizeof(struct trapframe));

  thread_fork(curproc->p_name, childProcess, (void *)&enter_forked_process, child_tf, 0);
  *retval = childProcess->process_id;

  return 0;
}




// Execv

int
sys_execv(const char *program, char **args) {

   (void) args; // change this later

// runprogram start

  struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

  // For program into Kernel Stack

  char * program_kernel = kmalloc((strlen(program) + 1) * sizeof(char));
  if (program_kernel == NULL) {
    return ENOMEM;
  }

  int copy_ans = copyin((const_userptr_t) program,  (void *) program_kernel, (strlen(program) + 1) * sizeof(char));
  
  // kprintf(program_kernel);

  // Add error statement
  if (copy_ans != 0) {
    return EIO;
  }

	/* Open the file. */
	result = vfs_open(program_kernel, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	// KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

// runprogram finish


}

#endif
