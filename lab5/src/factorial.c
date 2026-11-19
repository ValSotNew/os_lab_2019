#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FactorialArgs {
  long long begin;
  long long end;
  int mod;
};

static long long result;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void CheckPthread(int error, const char *operation) {
  if (error != 0) {
    fprintf(stderr, "%s: %s\n", operation, strerror(error));
    exit(1);
  }
}

static bool ReadNumber(const char *text, int *value) {
  if (text[0] < '0' || text[0] > '9') {
    return false;
  }

  char *end;
  errno = 0;
  long number = strtol(text, &end, 10);

  if (errno == ERANGE || *end != '\0' || number > INT_MAX) {
    return false;
  }

  *value = (int)number;
  return true;
}

static void *CalculatePart(void *argument) {
  const struct FactorialArgs *args = argument;
  long long local = 1 % args->mod;

  for (long long number = args->begin; number < args->end; ++number) {
    local = (local * number) % args->mod;
  }

  CheckPthread(pthread_mutex_lock(&mutex), "pthread_mutex_lock");
  result = (result * local) % args->mod;
  CheckPthread(pthread_mutex_unlock(&mutex), "pthread_mutex_unlock");

  return NULL;
}

int main(int argc, char **argv) {
  int k = -1;
  int pnum = 0;
  int mod = 0;

  static const struct option options[] = {
      {"pnum", required_argument, NULL, 'p'},
      {"mod", required_argument, NULL, 'm'},
      {NULL, 0, NULL, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "k:", options, NULL)) != -1) {
    int value;
    if (option == '?' || !ReadNumber(optarg, &value)) {
      fprintf(stderr, "Arguments must be integers from 0 to %d\n", INT_MAX);
      return 1;
    }

    switch (option) {
      case 'k': k = value; break;
      case 'p': pnum = value; break;
      case 'm': mod = value; break;
    }
  }

  if (optind != argc || k < 0 || pnum == 0 || mod == 0) {
    fprintf(stderr, "Usage: %s -k N --pnum N --mod N\n", argv[0]);
    fprintf(stderr, "k >= 0, pnum >= 1, mod >= 1\n");
    return 1;
  }

  pthread_t *threads = calloc(pnum, sizeof(*threads));
  struct FactorialArgs *args = calloc(pnum, sizeof(*args));

  if (threads == NULL || args == NULL) {
    perror("calloc");
    free(threads);
    free(args);
    return 1;
  }

  result = 1 % mod;
  int base = k / pnum;
  int extra = k % pnum;
  long long begin = 1;
  int started = 0;

  for (; started < pnum; ++started) {
    long long end = begin + base + (started < extra);
    args[started].begin = begin;
    args[started].end = end;
    args[started].mod = mod;

    int error = pthread_create(&threads[started], NULL, CalculatePart,
                               &args[started]);
    if (error != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      break;
    }
    begin = end;
  }

  for (int i = 0; i < started; ++i) {
    CheckPthread(pthread_join(threads[i], NULL), "pthread_join");
  }

  CheckPthread(pthread_mutex_destroy(&mutex), "pthread_mutex_destroy");
  free(args);
  free(threads);

  if (started != pnum) {
    return 1;
  }

  printf("%d! mod %d = %lld\n", k, mod, result);
  return 0;
}