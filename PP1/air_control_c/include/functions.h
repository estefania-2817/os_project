#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <pthread.h>
#include <sys/types.h>

void InitTakeoffController(int *shared_pids, int shm_fd, const char *shm_name);
void *TakeOffsFunction(void *arg);
void MemoryCreate();
pid_t get_radio_pid(void);
pid_t get_ground_pid(void);
extern int *sh_memory; 
extern int shm_fd;
extern volatile sig_atomic_t sigusr2_received;

extern pthread_mutex_t state_lock;
extern pthread_mutex_t runway1_lock;
extern pthread_mutex_t runway2_lock;

#endif