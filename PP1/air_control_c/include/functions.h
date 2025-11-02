/* functions.h - declarations for air_control C implementation */
#ifndef AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_
#define AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_

#include <pthread.h>

/* shared memory variables (3 ints): [air_pid, radio_pid, ground_pid] */
extern int* shmem;
extern int shmem_fd;
extern char shm_name[];

/* shared state and functions */
extern volatile sig_atomic_t planes;
extern int takeoffs;
extern int total_takeoffs;

/* synchronization */
extern pthread_mutex_t startup_lock;

void MemoryCreate();
void SigHandler2(int signal);
void* TakeOffsFunction(void* arg);

#endif  // AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_
