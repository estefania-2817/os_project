#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "functions.h"

#define CONTROLLERS 5

int main() {
  /* 1) Create shared memory and store this process PID in position 0 */
  MemoryCreate();

  /* 2) Configure SIGUSR2 handler */
  signal(SIGUSR2, SigHandler2);

  /* Create threads immediately so they're available for detection */
  pthread_t threads[CONTROLLERS];
  for (int i = 0; i < CONTROLLERS; ++i) {
    if (pthread_create(&threads[i], NULL, TakeOffsFunction, NULL) != 0) {
      perror("pthread_create");
      exit(1);
    }
  }

  /* Give threads time to start before continuing with radio launch */
  usleep(100000); /* 100ms */

  /* 3) Launch radio as a child process. Child stores its pid in shmem[1]
     then execs the provided radio executable passing the shm name. */
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(1);
  }

  if (pid == 0) {
    /* child */
    /* write our pid into shared memory second position (radio pid) */
    if (shmem) shmem[1] = getpid();
    /* run radio executable - relative path from PP1 directory */
    execl("../radio/radio", "../radio/radio",
          shm_name, (char*)NULL);
    /* if exec fails */
    perror("execl radio");
    exit(1);
  }

  /* parent: shmem[1] will be set by the child before exec; wait a bit */
  sleep(1);

  for (int i = 0; i < CONTROLLERS; ++i) {
    pthread_join(threads[i], NULL);
  }

  /* Send termination signals to radio and ground_control */
  if (shmem && shmem[1] > 0) kill(shmem[1], SIGTERM);
  if (shmem && shmem[2] > 0) kill(shmem[2], SIGTERM);

  /* Give processes time to terminate gracefully */
  sleep(1);

  /* Cleanup: remove shared memory name */
  shm_unlink(shm_name);

  /* wait for child to finish */
  wait(NULL);

  return 0;
}

