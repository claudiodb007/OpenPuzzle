#include "openpuzzle/client/ClientIdentity.hpp"

#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/AtomicFile.hpp"

#include <cerrno>
#include <cstdlib>
#include <locale>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace openpuzzle::client {

namespace {

std::filesystem::path identityPath() {
  const char* home =
      std::getenv("HOME");

  std::filesystem::path root =
      home
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  return root /
         ".config" /
         "OpenPuzzle" /
         "client.id";
}

std::string generateUuid() {
  std::random_device randomDevice;
  std::mt19937_64 generator(
      randomDevice());

  std::uniform_int_distribution<unsigned int>
      distribution(0, 255);

  unsigned char bytes[16];

  for (auto& byte : bytes) {
    byte = static_cast<unsigned char>(
        distribution(generator));
  }

  /*
   * UUID versão 4.
   */
  bytes[6] =
      static_cast<unsigned char>(
          (bytes[6] & 0x0f) | 0x40);

  bytes[8] =
      static_cast<unsigned char>(
          (bytes[8] & 0x3f) | 0x80);

  std::ostringstream output;
  output.imbue(std::locale::classic());

  output
      << std::hex
      << std::setfill('0');

  for (int index = 0;
       index < 16;
       ++index) {
    output
        << std::setw(2)
        << static_cast<int>(
               bytes[index]);

    if (index == 3 ||
        index == 5 ||
        index == 7 ||
        index == 9) {
      output << '-';
    }
  }

  return output.str();
}

// Keep the lock file in place: unlinking it would allow another caller to
// lock a different inode while an earlier caller still owns this lock.
class Descriptor {
public:
  explicit Descriptor(int value) : value_(value) {}
  ~Descriptor() { if (value_ >= 0) ::close(value_); }
  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;
  int get() const { return value_; }
private:
  int value_;
};

bool regularPrivateFile(int descriptor) {
  struct stat metadata {};
  return ::fstat(descriptor, &metadata) == 0 && S_ISREG(metadata.st_mode) &&
         ::fchmod(descriptor, S_IRUSR | S_IWUSR) == 0;
}

bool validUuid(const std::string& identity) {
  if (identity.size() != 36) {
    return false;
  }
  for (std::size_t index = 0; index < identity.size(); ++index) {
    const char value = identity[index];
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      if (value != '-') return false;
    } else if (!((value >= '0' && value <= '9') ||
                 (value >= 'a' && value <= 'f') ||
                 (value >= 'A' && value <= 'F'))) {
      return false;
    }
  }
  return true;
}

std::string readIdentity(int descriptor) {
  struct stat metadata {};
  if (::fstat(descriptor, &metadata) != 0 || !S_ISREG(metadata.st_mode)) {
    return {};
  }
  // A UUID, optional CRLF and one extra byte to detect trailing data. Never
  // accept a valid first line followed by an incomplete or unrelated record.
  std::string contents;
  char buffer[39];
  while (contents.size() < sizeof(buffer)) {
    const ssize_t count = ::read(descriptor, buffer, sizeof(buffer) - contents.size());
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) return {};
    if (count == 0) break;
    contents.append(buffer, static_cast<std::size_t>(count));
  }
  if (!contents.empty() && contents.back() == '\n') {
    contents.pop_back();
    if (!contents.empty() && contents.back() == '\r') contents.pop_back();
  }
  return validUuid(contents) ? contents : std::string{};
}

} // namespace

std::string ClientIdentity::loadOrCreate() {
  const auto path = identityPath();
  try {
    WorkspaceSecurity::prepare(path.parent_path());

    const Descriptor lock(::open(
        (path.string() + ".lock").c_str(),
        O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
        S_IRUSR | S_IWUSR));
    if (lock.get() < 0 || !regularPrivateFile(lock.get())) {
      return {};
    }
    int locked;
    do {
      locked = ::flock(lock.get(), LOCK_EX);
    } while (locked != 0 && errno == EINTR);
    if (locked != 0) return {};

    // Re-read under the lock, so all concurrent first callers observe the
    // same published identity. Only ENOENT permits creating a new UUID.
    const Descriptor input(::open(
        path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (input.get() >= 0) {
      const auto identity = readIdentity(input.get());
      if (identity.empty() || !regularPrivateFile(input.get())) return {};
      return identity;
    }
    if (errno != ENOENT) return {};

    const auto identity = generateUuid();
    return writePrivateFileAtomically(path, identity + "\n")
        ? identity : std::string{};
  } catch (...) {
    return {};
  }
}

} // namespace openpuzzle::client
