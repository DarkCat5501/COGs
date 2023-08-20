#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  printf("hello saylor %lu\n", time(NULL));

  return 0;
}
