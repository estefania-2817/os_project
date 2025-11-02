// I ADDED ALL INCLUDES AND DEFINES
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/functions.h"

#define SH_MEMORY_NAME "/takeoff_shm"
#define TOTAL_TAKEOFFS 20

// locks
pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t runway1_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t runway2_lock = PTHREAD_MUTEX_INITIALIZER;

int *sh_memory  = NULL;
int shm_fd = -1;
int planes = 0;
int takeoffs = 0;
int total_takeoffs = 0;
extern volatile sig_atomic_t sigusr2_received;


pid_t get_radio_pid(void) { return (pid_t) (sh_memory ? sh_memory[1] : 0); }
pid_t get_ground_pid(void){ return (pid_t) (sh_memory ? sh_memory[2] : 0); }

void MemoryCreate() {
  // TODO2: create the shared memory segment, configure it and store the PID of
  // the process in the first position of the memory block.
  shm_fd = shm_open(SH_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open");
    exit(1);
  }
  if (ftruncate(shm_fd, 3 * sizeof(int)) == -1) {
    perror("ftruncate");
    exit(1);
  }

  sh_memory  = mmap(NULL, sizeof(int) * 3, PROT_READ | PROT_WRITE, 
                      MAP_SHARED, shm_fd, 0);
  if (sh_memory == MAP_FAILED) {
      perror("mmap");
      exit(1);
  }
    
  // air_control PID
  sh_memory[0] = getpid();
  sh_memory[1] = 0; // radio PID 
  sh_memory[2] = 0; // ground control PID 
}

void* TakeOffsFunction(void *arg) {
  // TODO: implement the logic to control a takeoff thread
  //    Use a loop that runs while total_takeoffs < TOTAL_TAKEOFFS
  //    Use runway1_lock or runway2_lock to simulate a locked runway
  //    Use state_lock for safe access to shared variables (planes,
  //    takeoffs, total_takeoffs)
  //    Simulate the time a takeoff takes with sleep(1)
  //    Send SIGUSR1 every 5 local takeoffs
  //    Send SIGTERM when the total takeoffs target is reached

    while (1) {
        // Check total takeoffs first with lock
        pthread_mutex_lock(&state_lock);
        if (total_takeoffs >= TOTAL_TAKEOFFS) {
            // Important: Send SIGTERM only once when we reach exactly TOTAL_TAKEOFFS
            if (total_takeoffs == TOTAL_TAKEOFFS) {
                kill(get_radio_pid(), SIGTERM);
            }
            pthread_mutex_unlock(&state_lock);
            break;
        }
        
        // Process any pending SIGUSR2 signals
        if (sigusr2_received) {
            planes += 5;
            sigusr2_received = 0;
        }
        
        // If no planes available, release lock and wait
        if (planes <= 0) {
            pthread_mutex_unlock(&state_lock);
            usleep(1000);
            continue;
        }
        pthread_mutex_unlock(&state_lock);

        // Try to get a runway
        int have_runway = 0;
        if (pthread_mutex_trylock(&runway1_lock) == 0) {
            have_runway = 1;
        } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
            have_runway = 2;
        } else {
            usleep(1000);
            continue;
        }

        // Process takeoff
        pthread_mutex_lock(&state_lock);
        if (planes > 0 && total_takeoffs < TOTAL_TAKEOFFS) {
            planes--;
            takeoffs++;
            total_takeoffs++;
            
            // Send SIGUSR1 every 5 takeoffs
            if (takeoffs == 5) {
                kill(get_radio_pid(), SIGUSR1);
                takeoffs = 0;
            }
        }
        pthread_mutex_unlock(&state_lock);

        // Simulate takeoff time
        sleep(1);

        // Release runway
        if (have_runway == 1) {
            pthread_mutex_unlock(&runway1_lock);
        } else if (have_runway == 2) {
            pthread_mutex_unlock(&runway2_lock);
        }
    }
    return NULL;
}