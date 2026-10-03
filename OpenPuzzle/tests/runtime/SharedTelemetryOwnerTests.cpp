#include "openpuzzle/runtime/SharedTelemetryOwner.hpp"

#include <cassert>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  openpuzzle::SharedTelemetryOwner owner;
  assert(owner.index() == 0);

  int ready[2];
  int go[2];
  assert(pipe(ready) == 0);
  assert(pipe(go) == 0);

  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    close(ready[0]);
    close(go[1]);
    const bool initiallyNotOwner = owner.index() != 1;
    const char signal = 'r';
    if (write(ready[1], &signal, 1) != 1) {
      _exit(2);
    }
    char received = 0;
    if (read(go[0], &received, 1) != 1) {
      _exit(3);
    }
    _exit(initiallyNotOwner && owner.index() == 1 ? 0 : 4);
  }

  close(ready[1]);
  close(go[0]);
  char received = 0;
  assert(read(ready[0], &received, 1) == 1);
  assert(owner.index() == 0);
  owner.promote(1);
  assert(owner.index() == 1);
  const char signal = 'g';
  assert(write(go[1], &signal, 1) == 1);

  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  close(ready[0]);
  close(go[1]);
}
