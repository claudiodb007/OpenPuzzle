#pragma once

namespace openpuzzle {

class DaemonRunner {
public:
  DaemonRunner();

  int run(int ticks = 3);

  void stop();

private:
  void tick();

  bool running_ = false;
  int tickCount_ = 0;
};

} // namespace openpuzzle
