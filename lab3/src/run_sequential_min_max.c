#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s seed arraysize\n", argv[0]);
    return 1;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    execl("./sequential_min_max", "sequential_min_max",
          argv[1], argv[2], (char *)NULL);
    perror("execl");
    _exit(127);
  }

  int status;
  pid_t waited;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited == -1 && errno == EINTR);

  if (waited == -1) {
    perror("waitpid");
    return 1;
  }
  if (!WIFEXITED(status)) {
    fprintf(stderr, "sequential_min_max terminated abnormally\n");
    return 1;
  }
  return WEXITSTATUS(status);
}