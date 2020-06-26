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

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

#if OPT_A2

  // if (curproc->parentProcessPointer != NULL) {

  //   curproc->code_exit = _MKWAIT_EXIT(exitcode);

  //   lock_acquire(curproc->lock_waitexit);
  //   cv_signal(curproc->cv_waitexit, curproc->parentProcessPointer->lock_waitexit);
  //   lock_release(curproc->lock_waitexit);
  // }

  if (curproc->parentProcessPointer != NULL) {


    lock_acquire(curproc->lock_waitexit);
    curproc->terminated = true;
    curproc->code_exit = exitcode;
    struct proc *temp = NULL;

    for (int i = array_num(curproc->numberofChildProcess) - 1; i >= 0; i--) {
      temp = array_get(curproc->numberofChildProcess, i);
      temp->parentProcessPointer = NULL;
      if (temp->terminated) {
        proc_destroy(temp);
      }
      array_remove(curproc->numberofChildProcess, i);
    }
    
    lock_release(curproc->lock_waitexit);

    cv_signal(curproc->cv_waitexit, curproc->parentProcessPointer->lock_waitexit);
    

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
  
  // MAYBE WRONG

  // if (p->parentProcessPointer == NULL) {
    proc_destroy(p);
  // }  
  
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

  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2


//   lock_acquire(curproc->lock_waitexit);
//   bool child_exists = false;

//   for (unsigned int i = 0; i < array_num(curproc->numberofChildProcess); i++) {
//     struct proc *childp = array_get(curproc->numberofChildProcess, i);
//     if (childp->process_id == pid) {

//       child_exists = true;
//       // lock_acquire(childp->lock_waitexit);
//       while (childp->terminated == false) {
//         cv_wait(childp->cv_waitexit, curproc->lock_waitexit);        
//       }
//       // lock_release(childp->lock_waitexit);
//       exitstatus = _MKWAIT_EXIT(childp->code_exit);
//     }
//   } 

//   if (!child_exists) {
//     // No child process with this PID exists for the curthread
//     lock_release(curproc->lock_waitexit);
//     *retval = -1;
//     return ESRCH;
//   }

//   lock_release(curproc->lock_waitexit);







  lock_acquire(curproc->lock_waitexit);
  bool child_exists = false;

  struct proc *childp = NULL;

  for (int i = array_num(curproc->numberofChildProcess) - 1; i >= 0; i--) {
    childp =  array_get(curproc->numberofChildProcess, i);
    
    if (childp->process_id == pid) {
      child_exists = true;
      break;
    }
  } 

  if (!child_exists) {
    // No child process with this PID exists for the curthread
    lock_release(curproc->lock_waitexit);
    *retval = -1;
    return ESRCH;
  }


  while (childp->terminated == false) {
    cv_wait(childp->cv_waitexit, curproc->lock_waitexit);        
  }

  exitstatus = _MKWAIT_EXIT(childp->code_exit);



  lock_release(curproc->lock_waitexit);






#endif

  /* for now, just pretend the exitstatus is 0 */
  // exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


// ****************************************************************
// FORK
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
  lock_acquire(curproc->lock_waitexit);
	childProcess->p_addrspace = childAddrspace;
	
  // Assign exitcode
  //childProcess->code_exit = -1;

  // Assigning parent process pointer
  childProcess->parentProcessPointer = curproc;
  
  // Adding this child process to the parent's process child  
  int is_error = array_add(curproc->numberofChildProcess, childProcess, NULL);
  if(is_error) panic("Unable to copy child");

  lock_release(curproc->lock_waitexit);


  struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  memcpy(child_tf, tf, sizeof(struct trapframe));

  thread_fork(curproc->p_name, childProcess, (void *)&enter_forked_process, child_tf, 0);
  *retval = childProcess->process_id;

  return 0;
}


#endif
