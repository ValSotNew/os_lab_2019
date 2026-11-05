#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "find_min_max.h"

struct Worker {
  pid_t pid;
  FILE *input;
};

static int PositiveInt(const char *text) {
  char *end;
  errno = 0;
  long value = strtol(text, &end, 10);

  if (errno == ERANGE || end == text || *end != '\0' ||
      value < 1 || value > INT_MAX) {
    return 0;
  }
  return (int)value;
}

static void WriteResult(FILE *output, struct MinMax result) {
  int written = fprintf(output, "%d %d\n", result.min, result.max);
  int closed = fclose(output);
  _exit(written < 0 || closed == EOF ? 1 : 0);
}

static bool StartFileWorker(int *array, int begin, int end,
                            struct Worker *worker) {
  FILE *file = tmpfile();
  if (file == NULL) {
    perror("tmpfile");
    return false;
  }
  setbuf(file, NULL);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    fclose(file);
    return false;
  }

  if (pid == 0) {
    WriteResult(file, GetMinMax(array, begin, end));
  }

  worker->pid = pid;
  worker->input = file;
  return true;
}

static bool StartPipeWorker(int *array, int begin, int end,
                            struct Worker *worker) {
  int fd[2];
  if (pipe(fd) == -1) {
    perror("pipe");
    return false;
  }

  FILE *input = fdopen(fd[0], "r");
  if (input == NULL) {
    perror("fdopen");
    close(fd[0]);
    close(fd[1]);
    return false;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    fclose(input);
    close(fd[1]);
    return false;
  }

  if (pid == 0) {
    fclose(input);

    FILE *output = fdopen(fd[1], "w");
    if (output == NULL) {
      perror("fdopen");
      close(fd[1]);
      _exit(1);
    }

    WriteResult(output, GetMinMax(array, begin, end));
  }

  close(fd[1]);
  worker->pid = pid;
  worker->input = input;
  return true;
}

static bool CollectResult(struct Worker worker, bool with_files,
                          struct MinMax *result) {
  int status;
  pid_t waited;
  do {
    waited = waitpid(worker.pid, &status, 0);
  } while (waited == -1 && errno == EINTR);

  bool ok = waited == worker.pid && WIFEXITED(status) &&
            WEXITSTATUS(status) == 0;
  if (ok) {
    ok = (!with_files || fseek(worker.input, 0, SEEK_SET) == 0) &&
         fscanf(worker.input, "%d %d", &result->min, &result->max) == 2;
  }

  fclose(worker.input);
  return ok;
}

int main(int argc, char **argv) {
  int seed = 0;
  int array_size = 0;
  int pnum = 0;
  bool with_files = false;

  static const struct option options[] = {
      {"seed", required_argument, NULL, 's'},
      {"array_size", required_argument, NULL, 'a'},
      {"pnum", required_argument, NULL, 'p'},
      {"by_files", no_argument, NULL, 'f'},
      {NULL, 0, NULL, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "f", options, NULL)) != -1) {
    switch (option) {
      case 's': seed = PositiveInt(optarg); break;
      case 'a': array_size = PositiveInt(optarg); break;
      case 'p': pnum = PositiveInt(optarg); break;
      case 'f': with_files = true; break;
      default: return 1;
    }
  }

  if (optind != argc || seed == 0 || array_size == 0 || pnum == 0) {
    fprintf(stderr, "Usage: %s --seed N --array_size N --pnum N [--by_files]\n",
            argv[0]);
    return 1;
  }

  int *array = calloc(array_size, sizeof(*array));
  struct Worker *workers = calloc(pnum, sizeof(*workers));
  if (array == NULL || workers == NULL) {
    perror("calloc");
    free(array);
    free(workers);
    return 1;
  }
  GenerateArray(array, array_size, seed);

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  int base = array_size / pnum;
  int extra = array_size % pnum;
  int begin = 0;
  int started = 0;

  for (; started < pnum; ++started) {
    int end = begin + base + (started < extra);

    bool launched;
    if (with_files) {
      launched = StartFileWorker(array, begin, end, &workers[started]);
    } else {
      launched = StartPipeWorker(array, begin, end, &workers[started]);
    }

    if (!launched) break;
    begin = end;
  }

  bool ok = started == pnum;
  struct MinMax total = {INT_MAX, INT_MIN};

  for (int i = 0; i < started; ++i) {
    struct MinMax local;
    if (!CollectResult(workers[i], with_files, &local)) {
      fprintf(stderr, "Could not read result from worker %d\n", i);
      ok = false;
      continue;
    }

    if (local.min < total.min) total.min = local.min;
    if (local.max > total.max) total.max = local.max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  free(array);
  free(workers);
  if (!ok) return 1;

  double elapsed_ms = (finish_time.tv_sec - start_time.tv_sec) * 1000.0
                    + (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  printf("Min: %d\nMax: %d\n", total.min, total.max);
  printf("Elapsed time: %fms\n", elapsed_ms);
  return 0;
}