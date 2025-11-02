#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#define SH_MEMORY_NAME "/takeoff_shm"
#define PLANES_LIMIT 20

int planes = 0;
int takeoffs = 0;
int traffic = 0;

int* shared_memory = NULL;
int shm_fd = -1;

void Traffic(int signum) {
  // TODO:
  // Calculate the number of waiting planes.
  // Check if there are 10 or more waiting planes to send a signal and increment
  // planes. Ensure signals are sent and planes are incremented only if the
  // total number of planes has not been exceeded.
  (void)signum;

  if (planes >= PLANES_LIMIT) {
    printf( "All %d planes have been added. Stopping ground control...\n", PLANES_LIMIT);
    kill(getpid(), SIGTERM);
    return;
  }

  int waiting_planes = planes - takeoffs;
  if (waiting_planes >= 10) {
    printf("RUNWAY OVERLOADED, waiting planes in line:10\n");
  }

  // only add new planes if below limit
  if (planes + 5 <= PLANES_LIMIT) {
    planes += 5;
    printf("Planes: %d\n", planes);
    if (shared_memory != NULL && shared_memory[1] > 0) {
      kill(shared_memory[1], SIGUSR2);  // tell radio new planes arrived
    }
  }
}

void SigTermHandler(int signum) {
  /* cleanup */
  if (shared_memory) {
    munmap(shared_memory, 3 * sizeof(int));
  }
  close(shm_fd); /* if kept open */
  printf("Fin de operaciones...\n");
  exit (0);
}

void SigUsr1Handler(int signum) {
  (void)signum;
  takeoffs += 5;
  if (takeoffs >= PLANES_LIMIT) {
    printf("All %d takeoffs completed. Stopping ground control...\n", PLANES_LIMIT);
    kill(getpid(), SIGTERM);
  }
}

int main(int argc, char* argv[]) {
  // TODO:
  // 1. Open the shared memory block and store this process PID in position 2
  //    of the memory block.
  shm_fd = shm_open(SH_MEMORY_NAME, O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open");
    exit(1);
  }

  shared_memory = mmap(NULL, sizeof(int) * 3, PROT_READ | PROT_WRITE, 
                      MAP_SHARED, shm_fd, 0);
  if (shared_memory == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  shared_memory[2] = getpid();

  // 3. Configure SIGTERM and SIGUSR1 handlers
  //    - The SIGTERM handler should: close the shared memory, print
  //      "finalization of operations..." and terminate the program.
  //    - The SIGUSR1 handler should: increase the number of takeoffs by 5.
  signal(SIGTERM, SigTermHandler);
  signal(SIGUSR1, SigUsr1Handler); 

  // 2. Configure the timer to execute the Traffic function.
  struct itimerval timer;
  
  // Configure timer to fire every 500ms
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 500000;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 500000;
  
  if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
    perror("setitimer");
    exit(1);
  }
  
  // Set signal handler for timer
  signal(SIGALRM, Traffic);
  
  printf("ground_control started with PID: %d\n", getpid());
  
  // Keep the program running
  while (1) {
    pause();
  }
  
  return 0; 
}