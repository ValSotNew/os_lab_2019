#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sum.h"
#include "utils.h"

struct SumArgs {
  const int *array;
  size_t begin;
  size_t end;
  long long result;
};

static bool ParseUnsigned(const char *text, unsigned int *result) {
  if (text[0] < '0' || text[0] > '9') {
    return false;
  }

  char *end;
  errno = 0;
  unsigned long value = strtoul(text, &end, 10);
  if (errno == ERANGE || *end != '\0' || value > UINT_MAX) {
    return false;
  }

  *result = (unsigned int)value;
  return true;
}

static void PrintUsage(const char *program) {
  fprintf(stderr, "Usage: %s --threads_num N --seed N --array_size N\n",
          program);
}

static void *ThreadSum(void *argument) {
  struct SumArgs *args = argument;
  args->result = Sum(args->array, args->begin, args->end);
  return NULL;
}

int main(int argc, char **argv) {
  unsigned int threads_num = 0;
  unsigned int array_size = 0;
  unsigned int seed = 0;
  bool seed_given = false;

  static const struct option options[] = {
      {"threads_num", required_argument, NULL, 't'},
      {"seed", required_argument, NULL, 's'},
      {"array_size", required_argument, NULL, 'a'},
      {NULL, 0, NULL, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
    unsigned int value;
    if (option == '?' || !ParseUnsigned(optarg, &value)) {
      PrintUsage(argv[0]);
      return 1;
    }

    switch (option) {
      case 't':
        threads_num = value;
        break;
      case 's':
        seed = value;
        seed_given = true;
        break;
      case 'a':
        array_size = value;
        break;
    }
  }

  if (optind != argc || !seed_given || threads_num == 0 ||
      array_size == 0 || array_size > INT_MAX) {
    PrintUsage(argv[0]);
    return 1;
  }

  int *array = calloc(array_size, sizeof(*array));
  pthread_t *threads = calloc(threads_num, sizeof(*threads));
  struct SumArgs *args = calloc(threads_num, sizeof(*args));

  if (array == NULL || threads == NULL || args == NULL) {
    perror("calloc");
    free(args);
    free(threads);
    free(array);
    return 1;
  }

  GenerateArray(array, array_size, seed);

  size_t base = array_size / threads_num;
  size_t extra = array_size % threads_num;
  size_t begin = 0;

  struct timespec start;
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
    perror("clock_gettime");
    return 1;
  }

  unsigned int started = 0;
  for (; started < threads_num; ++started) {
    size_t end = begin + base + (started < extra);
    args[started].array = array;
    args[started].begin = begin;
    args[started].end = end;

    int error = pthread_create(&threads[started], NULL, ThreadSum, &args[started]);
    if (error != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      break;
    }
    begin = end;
  }

  long long total = 0;
  for (unsigned int i = 0; i < started; ++i) {
    int error = pthread_join(threads[i], NULL);
    if (error != 0) {
      fprintf(stderr, "pthread_join: %s\n", strerror(error));
      return 1;
    }
    total += args[i].result;
  }

  struct timespec finish;
  int clock_result = clock_gettime(CLOCK_MONOTONIC, &finish);
  if (clock_result == -1) {
    perror("clock_gettime");
  }

  free(args);
  free(threads);
  free(array);

  if (clock_result == -1 || started != threads_num) {
    return 1;
  }

  double elapsed_ms = (finish.tv_sec - start.tv_sec) * 1000.0
                    + (finish.tv_nsec - start.tv_nsec) / 1000000.0;
  printf("Total: %lld\n", total);
  printf("Elapsed time: %.3f ms\n", elapsed_ms);
  return 0;
}