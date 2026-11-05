#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHW_ADR(ID, ITEM) \
  printf("ID %-8s is at virtual address: %p\n", (ID), (void *)&(ITEM))

extern char etext, edata, end;

char *cptr = "This message is output by the function showit()\n";
char buffer1[25];

static int showit(const char *message);

int main(void) {
  int i = 0;

  printf("Address etext: %p\n", (void *)&etext);
  printf("Address edata: %p\n", (void *)&edata);
  printf("Address end  : %p\n", (void *)&end);

  SHW_ADR("main", main);
  SHW_ADR("showit", showit);
  SHW_ADR("cptr", cptr);
  SHW_ADR("buffer1", buffer1);
  SHW_ADR("i", i);

  strcpy(buffer1, "A demonstration\n");
  fflush(stdout);
  if (write(STDOUT_FILENO, buffer1, strlen(buffer1)) == -1) {
    perror("write");
    return 1;
  }

  return showit(cptr);
}

static int showit(const char *message) {
  char *buffer2;
  SHW_ADR("buffer2", buffer2);

  buffer2 = malloc(strlen(message) + 1);
  if (buffer2 == NULL) {
    perror("malloc");
    return 1;
  }

  printf("Allocated memory at: %p\n", (void *)buffer2);
  strcpy(buffer2, message);
  printf("%s", buffer2);
  free(buffer2);
  return 0;
}