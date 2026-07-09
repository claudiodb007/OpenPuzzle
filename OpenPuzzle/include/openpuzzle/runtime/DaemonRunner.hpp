#pragma once

namespace openpuzzle {

class Database;

class DaemonRunner {
public:
  explicit DaemonRunner(Database& database);

  int run(int ticks = 3);

  void stop();

private:
  void tick();

  Database& database_;
  bool running_ = false;
  int tickCount_ = 0;
};

} // namespace openpuzzle
