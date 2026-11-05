#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  pid_t child_pid = fork();

  if (child_pid == -1) {
    perror("fork");
    return 1;
  }

  if (child_pid == 0) {
    _exit(42);
  }

  siginfo_t info;
  int ready;
  do {
    ready = waitid(P_PID, child_pid, &info, WEXITED | WNOWAIT);
  } while (ready == -1 && errno == EINTR);

  if (ready == -1) {
    perror("waitid");
    return 1;
  }

  printf("Parent PID: %ld\n", (long)getpid());
  printf("Zombie PID: %ld\n", (long)child_pid);
  printf("Press Enter to collect the child's exit status.\n");
  fflush(stdout);
  (void)getchar();

  int status;
  pid_t waited;
  do {
    waited = waitpid(child_pid, &status, 0);
  } while (waited == -1 && errno == EINTR);

  if (waited == -1) {
    perror("waitpid");
    return 1;
  }

  if (WIFEXITED(status)) {
    printf("Child exit code: %d\n", WEXITSTATUS(status));
  }

  printf("Child %ld has been reaped.\n", (long)child_pid);
  return 0;
}