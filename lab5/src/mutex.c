#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int common = 0;

#ifdef USE_MUTEX
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
#endif

static void CheckPthread(int error, const char *operation) {
  if (error != 0) {
    fprintf(stderr, "%s: %s\n", operation, strerror(error));
    exit(1);
  }
}

static void *do_one_thing(void *argument) {
  int *pnum_times = argument;

  for (int i = 0; i < 50; ++i) {
#ifdef USE_MUTEX
    CheckPthread(pthread_mutex_lock(&mut), "pthread_mutex_lock");
#endif

    printf("doing one thing\n");
    int work = *pnum_times;
    printf("counter = %d\n", work);
    work++;

    for (unsigned long k = 0; k < 500000; ++k) {
    }

    *pnum_times = work;

#ifdef USE_MUTEX
    CheckPthread(pthread_mutex_unlock(&mut), "pthread_mutex_unlock");
#endif
  }

  return NULL;
}

static void *do_another_thing(void *argument) {
  int *pnum_times = argument;

  for (int i = 0; i < 50; ++i) {
#ifdef USE_MUTEX
    CheckPthread(pthread_mutex_lock(&mut), "pthread_mutex_lock");
#endif

    printf("doing another thing\n");
    int work = *pnum_times;
    printf("counter = %d\n", work);
    work++;

    for (unsigned long k = 0; k < 500000; ++k) {
    }

    *pnum_times = work;

#ifdef USE_MUTEX
    CheckPthread(pthread_mutex_unlock(&mut), "pthread_mutex_unlock");
#endif
  }

  return NULL;
}

int main(void) {
  pthread_t thread1;
  pthread_t thread2;

  CheckPthread(pthread_create(&thread1, NULL, do_one_thing, &common),
               "pthread_create");
  CheckPthread(pthread_create(&thread2, NULL, do_another_thing, &common),
               "pthread_create");

  CheckPthread(pthread_join(thread1, NULL), "pthread_join");
  CheckPthread(pthread_join(thread2, NULL), "pthread_join");

#ifdef USE_MUTEX
  CheckPthread(pthread_mutex_destroy(&mut), "pthread_mutex_destroy");
#endif

  printf("All done, counter = %d\n", common);
  return 0;
}