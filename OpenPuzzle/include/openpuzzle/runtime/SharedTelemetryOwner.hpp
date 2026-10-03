#pragma once

#include <atomic>
#include <new>
#include <stdexcept>
#include <sys/mman.h>

namespace openpuzzle {

// The supervisor changes ownership only after waitpid has reaped the previous
// owner. The anonymous mapping is inherited by the forked workers and never
// appears in the client state directory.
class SharedTelemetryOwner {
public:
  SharedTelemetryOwner() {
    static_assert(std::atomic<int>::is_always_lock_free);

    void* mapping = mmap(
        nullptr,
        sizeof(std::atomic<int>),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0);

    if (mapping == MAP_FAILED) {
      throw std::runtime_error(
          "Unable to create shared GPU telemetry ownership");
    }

    owner_ = new (mapping) std::atomic<int>(0);
  }

  SharedTelemetryOwner(const SharedTelemetryOwner&) = delete;
  SharedTelemetryOwner& operator=(const SharedTelemetryOwner&) = delete;

  ~SharedTelemetryOwner() {
    owner_->~atomic();
    munmap(owner_, sizeof(std::atomic<int>));
  }

  const std::atomic<int>* state() const {
    return owner_;
  }

  void promote(int index) {
    owner_->store(index, std::memory_order_release);
  }

  int index() const {
    return owner_->load(std::memory_order_acquire);
  }

private:
  std::atomic<int>* owner_ = nullptr;
};

} // namespace openpuzzle
