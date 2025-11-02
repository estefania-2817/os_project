
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Shared state */
volatile sig_atomic_t planes = 0;  /* Total planes arrived */
volatile sig_atomic_t waiting_planes = 0;  /* Planes waiting for takeoff */
int takeoffs = 0;
int total_takeoffs = 0;
time_t start_time = 0;

#define TOTAL_TAKEOFFS 20
#define TIMEOUT_SECONDS 15

/* Synchronization primitives */
pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t runway1_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t runway2_lock = PTHREAD_MUTEX_INITIALIZER;

/* Thread startup synchronization */
static int threads_started = 0;
pthread_mutex_t startup_lock = PTHREAD_MUTEX_INITIALIZER;

/* Shared memory bookkeeping */
int* shmem = NULL; /* pointer to mapped shared memory (3 ints) */
int shmem_fd = -1;
char shm_name[64];

void MemoryCreate() {
  snprintf(shm_name, sizeof(shm_name), "/shm_pids_%d", getpid());

  shmem_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
  if (shmem_fd == -1) {
    perror("shm_open");
    exit(1);
  }

  if (ftruncate(shmem_fd, sizeof(int) * 3) == -1) {
    perror("ftruncate");
    shm_unlink(shm_name);
    exit(1);
  }

  shmem = mmap(NULL, sizeof(int) * 3, PROT_READ | PROT_WRITE, MAP_SHARED,
               shmem_fd, 0);
  if (shmem == MAP_FAILED) {
    perror("mmap");
    shm_unlink(shm_name);
    exit(1);
  }

  /* store this process pid in first position */
  shmem[0] = getpid();
}

/* Signal handler: increment planes by 5 when SIGUSR2 received */
void SigHandler2(int signal) {
  planes += 5;
  waiting_planes += 5;
}

void* TakeOffsFunction(void* arg) {
  (void)arg;

  /* Initialize start time on first thread */
  pthread_mutex_lock(&state_lock);
  if (start_time == 0) {
    start_time = time(NULL);
  }
  pthread_mutex_unlock(&state_lock);

  while (1) {
    /* Check termination conditions */
    pthread_mutex_lock(&state_lock);
    time_t current_time = time(NULL);
    if (total_takeoffs >= TOTAL_TAKEOFFS ||
        (current_time - start_time) > TIMEOUT_SECONDS) {
      pthread_mutex_unlock(&state_lock);
      break;
    }
    pthread_mutex_unlock(&state_lock);

    int got_runway = 0;
    pthread_mutex_t* used_runway = NULL;

    /* Try to acquire one of the two runways */
    if (pthread_mutex_trylock(&runway1_lock) == 0) {
      got_runway = 1;
      used_runway = &runway1_lock;
    } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
      got_runway = 1;
      used_runway = &runway2_lock;
    } else {
      /* No runway available, wait a short time and retry */
      usleep(10000); /* 10ms - longer wait to keep threads visible */
      continue;
    }

    /* We have a runway, try to perform a takeoff if any planes waiting */
    pthread_mutex_lock(&state_lock);
    int did_takeoff = 0;
    if (waiting_planes > 0 && total_takeoffs < TOTAL_TAKEOFFS) {
      waiting_planes -= 1;  /* Decrement waiting planes only */
      takeoffs += 1;
      total_takeoffs += 1;
      did_takeoff = 1;

      /* If we've reached a block of 5 takeoffs notify radio */
      if (takeoffs == 5) {
        /* radio pid is stored in shmem[1] */
        if (shmem && shmem[1] > 0) kill(shmem[1], SIGUSR1);
        takeoffs = 0;
      }
    }
    pthread_mutex_unlock(&state_lock);

    /* Simulate takeoff time or runway occupation */
    if (did_takeoff) {
      sleep(1);
    } else {
      usleep(100000); /* 100ms when no takeoff */
    }

    /* Release the runway */
    if (used_runway) pthread_mutex_unlock(used_runway);
  }

  /* cleanup mapping and fd in the caller (main) will unlink */
  return NULL;
}