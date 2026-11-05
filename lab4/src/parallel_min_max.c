#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "find_min_max.h"

struct Worker {
  pid_t pid;
  FILE *input;
};

static volatile sig_atomic_t timeout_expired = 0;

static void HandleAlarm(int signum) {
  (void)signum;
  timeout_expired = 1;
}

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

static pid_t WaitForWorker(pid_t pid, int *status, bool use_timeout) {
  int options = use_timeout ? WNOHANG : 0;
  const struct timespec delay = {0, 10 * 1000 * 1000};

  for (;;) {
    pid_t waited = waitpid(pid, status, options);
    if (waited == -1 && errno == EINTR) continue;
    if (waited != 0) return waited;
    if (timeout_expired) return 0;

    nanosleep(&delay, NULL);
  }
}

static void StopWorkers(const struct Worker *workers, int first, int count) {
  for (int i = first; i < count; ++i) {
    if (kill(workers[i].pid, SIGKILL) == -1 && errno != ESRCH) {
      perror("kill");
    }
  }
}

static bool ReadResult(FILE *input, bool with_files, struct MinMax *result) {
  if (with_files && fseek(input, 0, SEEK_SET) != 0) {
    return false;
  }
  return fscanf(input, "%d %d", &result->min, &result->max) == 2;
}

int main(int argc, char **argv) {
  int seed = 0;
  int array_size = 0;
  int pnum = 0;
  int timeout = 0;
  bool with_files = false;

  static const struct option options[] = {
      {"seed", required_argument, NULL, 's'},
      {"array_size", required_argument, NULL, 'a'},
      {"pnum", required_argument, NULL, 'p'},
      {"by_files", no_argument, NULL, 'f'},
      {"timeout", required_argument, NULL, 't'},
      {NULL, 0, NULL, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "f", options, NULL)) != -1) {
    switch (option) {
      case 's': seed = PositiveInt(optarg); break;
      case 'a': array_size = PositiveInt(optarg); break;
      case 'p': pnum = PositiveInt(optarg); break;
      case 'f': with_files = true; break;
      case 't':
        timeout = PositiveInt(optarg);
        if (timeout == 0) {
          fprintf(stderr, "--timeout must be a positive integer in seconds\n");
          return 1;
        }
        break;
      default: return 1;
    }
  }

  if (optind != argc || seed == 0 || array_size == 0 || pnum == 0) {
    fprintf(stderr,
            "Usage: %s --seed N --array_size N --pnum N "
            "[--by_files] [--timeout SECONDS]\n", argv[0]);
    return 1;
  }

  if (timeout > 0 && signal(SIGALRM, HandleAlarm) == SIG_ERR) {
    perror("signal");
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
  if (timeout > 0) {
    alarm((unsigned int)timeout);
  }

  int base = array_size / pnum;
  int extra = array_size % pnum;
  int begin = 0;
  int started = 0;

  for (; started < pnum && !timeout_expired; ++started) {
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
  bool deadline_hit = timeout_expired && started < pnum;
  if (deadline_hit) {
    StopWorkers(workers, 0, started);
  }

  struct MinMax total = {INT_MAX, INT_MIN};
  for (int i = 0; i < started; ++i) {
    int status;
    pid_t pid = workers[i].pid;
    pid_t waited = WaitForWorker(pid, &status, timeout > 0 && !deadline_hit);

    if (waited == 0) {
      deadline_hit = true;
      StopWorkers(workers, i, started);
      waited = WaitForWorker(pid, &status, false);
    }

    if (waited == -1) {
      perror("waitpid");
      ok = false;
    } else if (!deadline_hit) {
      struct MinMax local;
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
          ReadResult(workers[i].input, with_files, &local)) {
        if (local.min < total.min) total.min = local.min;
        if (local.max > total.max) total.max = local.max;
      } else {
        fprintf(stderr, "Could not read result from worker %d\n", i);
        ok = false;
      }
    }

    fclose(workers[i].input);
  }

  if (timeout > 0) {
    alarm(0);
  }
  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);
  free(array);
  free(workers);

  if (deadline_hit) {
    fprintf(stderr, "Timeout after %d second(s)\n", timeout);
    return 124;
  }
  if (!ok) return 1;

  double elapsed_ms = (finish_time.tv_sec - start_time.tv_sec) * 1000.0
                    + (finish_time.tv_usec - start_time.tv_usec) / 1000.0;
  printf("Min: %d\nMax: %d\n", total.min, total.max);
  printf("Elapsed time: %fms\n", elapsed_ms);
  return 0;
}