#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

int main() {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(3, &mask);
  sched_setaffinity(0, sizeof(mask), &mask);
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid > 0) {
    sleep(100000000);
  }
  if (pid == 0) {
    uint64_t count = 0;
    srand(time(NULL));
    while (true) {
      if (rand() % 2 == 0) {
        count += 2;
      } else {
	count += 1;
      }
    }
  }
}
