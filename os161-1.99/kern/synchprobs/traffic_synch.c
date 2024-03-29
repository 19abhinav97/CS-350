#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
// static struct semaphore *intersectionSem;
static struct lock *intersectionLock;
static struct cv *north_cv;
static struct cv *south_cv;
static struct cv *east_cv;
static struct cv *west_cv;
int volatile vehicles_inside_intersection;
int directionArray[4];
volatile int number_of_cars[4];

// static struct queue vehicle_q;
// typedef struct Vehicles
// {
//   Direction origin;
//   Direction destination;
// } v_car;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  
  intersectionLock = lock_create("intersectionLock");
  if (intersectionLock == NULL) {
    panic("could not create intersection Lock");
  }
  north_cv = cv_create("north");
  if (north_cv == NULL) {
    panic("could not create North CV");
  }
  south_cv = cv_create("south");
  if (south_cv == NULL) {
    panic("could not create South CV");
  }
  east_cv = cv_create("east");
  if (east_cv == NULL) {
    panic("could not create East CV");
  }
  west_cv = cv_create("west");
  if (west_cv == NULL) {
    panic("could not create West CV");
  }
  
  
  for(int i = 0 ; i < 4; i ++) directionArray[i] = 0;
  for(int i = 0 ; i < 4; i ++) number_of_cars[i] = 0;
  vehicles_inside_intersection = 0;
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionLock != NULL);
  lock_destroy(intersectionLock);
  cv_destroy(north_cv);
  cv_destroy(south_cv);
  cv_destroy(east_cv);
  cv_destroy(west_cv);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  (void)destination;
  KASSERT(intersectionLock != NULL);

  lock_acquire(intersectionLock);
  number_of_cars[origin] += 1;

  // v_car.origin = origin;
  // v_car.destination = destination;

  if (origin == north) {
    if (directionArray[1] || directionArray[2] || directionArray[3]) {
      cv_wait(north_cv, intersectionLock);
    }
    directionArray[0] = 1;
  }
  if (origin == south) {
    if (directionArray[0] || directionArray[2] || directionArray[3]) {
      cv_wait(south_cv, intersectionLock);
    }
    directionArray[1] = 1;
  }
  if (origin == east) {
    if ((directionArray[0] != 0) || (directionArray[1] != 0) || (directionArray[3] != 0)) {
      cv_wait(east_cv, intersectionLock);
    }
    directionArray[2] = 1;
  }
  if (origin == west) {
    if ((directionArray[0] != 0) || (directionArray[1] != 0) || (directionArray[2] != 0)) {
      cv_wait(west_cv, intersectionLock);
    }
    directionArray[3] = 1;
  }
  number_of_cars[origin]-=1;
  vehicles_inside_intersection+=1;
  lock_release(intersectionLock);
}

int max_index(volatile int a[]) {
  int temp = 0;
  int index = -1;
  for (int i = 0; i < 4; ++i) {
    if (a[i] > temp) {
      temp = a[i];
      index = i;
    }
  }
  return index;
}

/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.1
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);

  if (origin == north) {
    
    if (directionArray[1] == 0 || directionArray[2] == 0 || directionArray[3] == 0)
    {
      
      vehicles_inside_intersection-=1;
      if(vehicles_inside_intersection == 0){
        directionArray[0] = 0;
        
        int index = max_index(number_of_cars);

        if (index == 1) {
          directionArray[1] = 1;
          cv_broadcast(south_cv, intersectionLock);
        }
        if (index == 2) {
          directionArray[2] = 1;
          cv_broadcast(east_cv, intersectionLock);
        }
        if (index == 3) {
          directionArray[3] = 1;
          cv_broadcast(west_cv, intersectionLock);
        }
      }
    }
  }

  if (origin == south) {
    
    if (directionArray[0] == 0 || directionArray[2] == 0 || directionArray[3] == 0) {
      
      vehicles_inside_intersection-=1;
      if(vehicles_inside_intersection == 0){
      directionArray[1] = 0;
        
        int index = max_index(number_of_cars);

        if (index == 0) {
          directionArray[0] = 1;  
          cv_broadcast(north_cv, intersectionLock);
        }
        if (index == 2) {
          directionArray[2] = 1;
          cv_broadcast(east_cv, intersectionLock);
        }
        if (index == 3) {
          directionArray[3] = 1;
          cv_broadcast(west_cv, intersectionLock);
        }
      }
    }
  }

  if (origin == east) {
    
    if (directionArray[1] == 0 || directionArray[0] == 0 || directionArray[3] == 0) {

      vehicles_inside_intersection-=1;
      if(vehicles_inside_intersection == 0){
        directionArray[2] = 0;
        int index = max_index(number_of_cars);

        if (index == 1) {
          directionArray[1] = 1;
          cv_broadcast(south_cv, intersectionLock);
        }
        if (index == 0) {
          directionArray[0] = 1;
          cv_broadcast(north_cv, intersectionLock);
        }
        if (index == 3) {
          directionArray[3] = 1;
          cv_broadcast(west_cv, intersectionLock);
        }
      }
    }
  }

  if (origin == west) {
    
    if (directionArray[1] == 0 || directionArray[2] == 0 || directionArray[0] == 0) {

      vehicles_inside_intersection-=1;
      if(vehicles_inside_intersection == 0){
        directionArray[3] = 0;
        
        int index = max_index(number_of_cars);

        if (index == 1) {
          directionArray[1] = 1;
          cv_broadcast(south_cv, intersectionLock);
        }
        if (index == 2) {
          directionArray[2] = 1;
          cv_broadcast(east_cv, intersectionLock);
        }
        if (index == 0) {
          directionArray[0] = 1;
          cv_broadcast(north_cv, intersectionLock);
        }
      }
    }
  }
  lock_release(intersectionLock);
}
