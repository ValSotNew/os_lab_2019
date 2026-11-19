#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t barrier;

struct ThreadArgs {
  int number;
  pthread_mutex_t *first_mutex;
  pthread_mutex_t *second_mutex;
  char first_name;
  char second_name;
};

static void CheckPthread(int error, const char *operation) {
  if (error != 0) {
    fprintf(stderr, "%s: %s\n", operation, strerror(error));
    exit(EXIT_FAILURE);
  }
}

static void *LockBoth(void *argument) {
  const struct ThreadArgs *args = argument;

  CheckPthread(pthread_mutex_lock(args->first_mutex), "pthread_mutex_lock");
  printf("Thread %d acquired %c\n", args->number, args->first_name);

  int status = pthread_barrier_wait(&barrier);
  if (status != PTHREAD_BARRIER_SERIAL_THREAD) {
    CheckPthread(status, "pthread_barrier_wait");
  }

  printf("Thread %d is waiting for %c\n", args->number, args->second_name);
  CheckPthread(pthread_mutex_lock(args->second_mutex), "pthread_mutex_lock");

  CheckPthread(pthread_mutex_unlock(args->second_mutex), "pthread_mutex_unlock");
  CheckPthread(pthread_mutex_unlock(args->first_mutex), "pthread_mutex_unlock");
  return NULL;
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);

  pthread_t thread1;
  pthread_t thread2;
  struct ThreadArgs first = {1, &mutex_a, &mutex_b, 'A', 'B'};
  struct ThreadArgs second = {2, &mutex_b, &mutex_a, 'B', 'A'};

  CheckPthread(pthread_barrier_init(&barrier, NULL, 2), "pthread_barrier_init");
  CheckPthread(pthread_create(&thread1, NULL, LockBoth, &first), "pthread_create");
  CheckPthread(pthread_create(&thread2, NULL, LockBoth, &second), "pthread_create");

  CheckPthread(pthread_join(thread1, NULL), "pthread_join");
  CheckPthread(pthread_join(thread2, NULL), "pthread_join");

  CheckPthread(pthread_barrier_destroy(&barrier), "pthread_barrier_destroy");
  CheckPthread(pthread_mutex_destroy(&mutex_a), "pthread_mutex_destroy");
  CheckPthread(pthread_mutex_destroy(&mutex_b), "pthread_mutex_destroy");
  return 0;
}