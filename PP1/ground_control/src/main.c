/* ground_control main.c - Airport ground traffic control simulation */
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "functions.h"

int* shmem = NULL;
int shmem_fd = -1;
char shm_name[128];

int main() {
  signal(SIGTERM, HandleSigterm);
  printf("Ground control: Starting and waiting for shared memory...\n");

  char found[256] = "";

  // Wait for shared memory to be created by air_control
  for (int attempts = 0; attempts < 30; attempts++) {
    DIR *dir = opendir("/dev/shm");
    if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "shm_pids_", 9) == 0) {
          strncpy(found, entry->d_name, sizeof(found) - 1);
          break;
        }
      }
      closedir(dir);

      if (strlen(found) > 0) break;
    }

    printf("Ground control: Waiting for shared memory (attempt %d)...\n",
           attempts + 1);
    sleep(1);
  }

  if (strlen(found) == 0) {
    printf("Ground control: Timeout waiting for shared memory\n");
    exit(1);
  }

  // Connect to shared memory
  snprintf(shm_name, sizeof(shm_name), "/%s", found);
  printf("Ground control: Found shared memory: %s\n", shm_name);

  shmem_fd = shm_open(shm_name, O_RDWR, 0666);
  if (shmem_fd == -1) {
    perror("shm_open");
    exit(1);
  }

  shmem = mmap(NULL, 3 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED,
               shmem_fd, 0);
  if (shmem == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  // Register ground PID
  shmem[2] = getpid();
  printf("Ground control: Registered PID %d\n", getpid());

  // Wait for both air_control and radio to register PIDs
  pid_t air_pid = 0;
  pid_t radio_pid = 0;
  for (int attempts = 0; attempts < 25; attempts++) {
    air_pid = shmem[0];
    radio_pid = shmem[1];
    if (air_pid > 0 && radio_pid > 0) break;
    printf("Ground control: Waiting for PIDs (attempt %d)\n", attempts + 1);
    printf("  Air: %d Radio: %d\n", air_pid, radio_pid);
    sleep(1);
  }

  printf("Ground control: Air PID: %d, Radio PID: %d\n", air_pid, radio_pid);

  if (air_pid <= 0 || radio_pid <= 0) {
    printf("Ground control: Timeout waiting for process PIDs\n");
    exit(1);
  }

  // Additional wait to ensure all processes are fully initialized
  printf("Ground control: Waiting for processes to initialize...\n");
  sleep(3);

  // Send plane arrival signals to RADIO (not air_control) - SIGUSR2
  printf("Ground control: Sending plane arrivals to radio...\n");
  for (int i = 0; i < 4; i++) {
    if (kill(radio_pid, SIGUSR2) == 0) {
      printf("Ground control: Plane group %d sent to radio (+5 planes)\n",
             i + 1);
    } else {
      perror("kill radio SIGUSR2");
    }
    usleep(400000);  // 0.4 seconds between plane groups
  }

  // Brief pause before takeoff clearances
  sleep(2);

  // Send takeoff clearance signals to air_control - SIGUSR1
  printf("Ground control: Sending takeoff clearances to air_control...\n");
  for (int i = 0; i < 20; i++) {
    if (kill(air_pid, SIGUSR1) == 0) {
      printf("Ground control: Takeoff clearance %d sent\n", i + 1);
    } else {
      perror("kill air_control SIGUSR1");
    }
    usleep(200000);  // 0.2 seconds between clearances
  }

  printf("Ground control: All signals sent, waiting for termination...\n");

  // Keep running until terminated
  while (1) {
    sleep(1);
  }

  return 0;
}
