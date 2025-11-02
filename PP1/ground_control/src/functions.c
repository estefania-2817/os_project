/* functions.c - ground_control helper functions */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "functions.h"

extern int* shmem;
extern int shmem_fd;

void HandleSigterm(int signum) {
  (void)signum;
  printf("Ground control: Terminating...\n");
  if (shmem_fd != -1) close(shmem_fd);
  if (shmem != MAP_FAILED && shmem != NULL) {
    munmap(shmem, 3 * sizeof(int));
  }
  exit(0);
}