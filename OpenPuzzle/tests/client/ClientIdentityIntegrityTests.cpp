#include "openpuzzle/client/ClientIdentity.hpp"

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using openpuzzle::client::ClientIdentity;

struct GroupedNumbers : std::numpunct<char> {
  char do_thousands_sep() const override { return '_'; }
  std::string do_grouping() const override { return "\1"; }
};

std::string read(const fs::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}
void write(const fs::path& path, const std::string& bytes) {
  std::ofstream output(path);
  output << bytes;
  output.close();
  assert(output);
}
bool validUuid(const std::string& value) {
  if (value.size() != 36) return false;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char c = value[index];
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      if (c != '-') return false;
    } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}
void expectPrivate(const fs::path& path) {
  assert(fs::status(path).permissions() ==
      (fs::perms::owner_read | fs::perms::owner_write));
}
void waitSuccessful(pid_t child) {
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
void readExactly(int descriptor, char* buffer, std::size_t size) {
  while (size > 0) {
    const ssize_t count = ::read(descriptor, buffer, size);
    if (count < 0 && errno == EINTR) continue;
    assert(count > 0);
    buffer += count;
    size -= static_cast<std::size_t>(count);
  }
}
}

int main() {
  const char* previous = std::getenv("HOME");
  const std::string previousHome = previous ? previous : "";
  char pattern[] = "/tmp/openpuzzle-identity-integrity-XXXXXX";
  const char* temporary = ::mkdtemp(pattern);
  assert(temporary);
  const fs::path root(temporary);
  const auto useHome = [&](const std::string& name) {
    const auto home = root / name;
    fs::create_directories(home / ".config/OpenPuzzle");
    assert(setenv("HOME", home.c_str(), 1) == 0);
    return home / ".config/OpenPuzzle/client.id";
  };

  // Existing invalid or incomplete files are retained byte for byte. No
  // replacement UUID may silently disconnect outstanding work from its owner.
  auto path = useHome("invalid");
  for (const std::string& bytes : std::vector<std::string>{
           "", "partial-identity\n", "12345678-1234-1234-1234-123456789abg\n",
           "12345678-1234-1234-1234-123456789abc\nextra\n",
           "12345678-1234-1234-1234-123456789abc\n\n"}) {
    write(path, bytes);
    assert(ClientIdentity::loadOrCreate().empty());
    assert(read(path) == bytes);
  }

  // Current files, UUIDs without a newline and Windows CRLF files retain the
  // exact UUID, including uppercase hexadecimal in legacy identities.
  const std::string existing = "12345678-1234-1234-ABCD-123456789ABC";
  for (const auto& ending : {std::string{}, std::string{"\n"}, std::string{"\r\n"}}) {
    write(path, existing + ending);
    assert(ClientIdentity::loadOrCreate() == existing);
    assert(read(path) == existing + ending);
    expectPrivate(path);
  }

  // Numeric locale grouping must never become part of a UUID.
  path = useHome("locale");
  const auto oldLocale = std::locale();
  std::locale::global(std::locale(std::locale::classic(), new GroupedNumbers));
  const auto identity = ClientIdentity::loadOrCreate();
  std::locale::global(oldLocale);
  assert(validUuid(identity) && identity[14] == '4');
  assert(identity[19] == '8' || identity[19] == '9' ||
         identity[19] == 'a' || identity[19] == 'b');
  assert(read(path) == identity + "\n");
  assert(ClientIdentity::loadOrCreate() == identity);
  expectPrivate(path);
  expectPrivate(path.string() + ".lock");

  // A real short write must not publish a partial identity. A later successful
  // call creates one complete UUID; no temporary file survives the failure.
  path = useHome("short-write");
  auto child = fork();
  assert(child >= 0);
  if (child == 0) {
    assert(signal(SIGXFSZ, SIG_IGN) != SIG_ERR);
    const rlimit limit{10, 10};
    assert(setrlimit(RLIMIT_FSIZE, &limit) == 0);
    assert(ClientIdentity::loadOrCreate().empty());
    _exit(0);
  }
  waitSuccessful(child);
  assert(!fs::exists(path));
  for (const auto& entry : fs::directory_iterator(path.parent_path())) {
    assert(entry.path().filename().string().find(".tmp.") == std::string::npos);
  }
  assert(validUuid(ClientIdentity::loadOrCreate()));

  // Directories, FIFOs, identity symlinks and lock symlinks are controlled
  // failures. They neither block a caller on a FIFO nor alter their targets.
  path = useHome("directory");
  fs::create_directory(path);
  write(path / "keep", "preserve");
  assert(ClientIdentity::loadOrCreate().empty());
  assert(read(path / "keep") == "preserve");
  path = useHome("fifo");
  assert(::mkfifo(path.c_str(), 0600) == 0);
  assert(ClientIdentity::loadOrCreate().empty());
  assert(fs::is_fifo(path));
  path = useHome("symlink");
  const auto target = root / "target";
  write(target, existing + "\n");
  const auto targetPermissions = fs::status(target).permissions();
  fs::create_symlink(target, path);
  assert(ClientIdentity::loadOrCreate().empty());
  assert(read(target) == existing + "\n");
  assert(fs::status(target).permissions() == targetPermissions);
  path = useHome("lock-symlink");
  fs::create_symlink(target, path.string() + ".lock");
  assert(ClientIdentity::loadOrCreate().empty());
  assert(!fs::exists(path));
  assert(read(target) == existing + "\n");

  // Eight simultaneous first callers must return the same durable identity.
  path = useHome("concurrent");
  int gate[2], results[2];
  assert(pipe(gate) == 0 && pipe(results) == 0);
  std::vector<pid_t> children;
  for (int i = 0; i < 8; ++i) {
    child = fork();
    assert(child >= 0);
    if (child == 0) {
      close(gate[1]); close(results[0]);
      char begin;
      readExactly(gate[0], &begin, 1);
      const auto value = ClientIdentity::loadOrCreate();
      assert(validUuid(value));
      assert(::write(results[1], value.data(), value.size()) == 36);
      _exit(0);
    }
    children.push_back(child);
  }
  close(gate[0]); close(results[1]);
  assert(::write(gate[1], "xxxxxxxx", 8) == 8);
  close(gate[1]);
  std::string returned(8 * 36, '\0');
  readExactly(results[0], returned.data(), returned.size());
  close(results[0]);
  for (auto pid : children) waitSuccessful(pid);
  const auto common = returned.substr(0, 36);
  for (int i = 1; i < 8; ++i) assert(returned.substr(i * 36, 36) == common);
  assert(read(path) == common + "\n");

  // An existing lock is respected, including for reading an already stored
  // identity. Another cooperating caller cannot replace or read midway through
  // creation. The lock inode remains stable after the owner releases it.
  const auto lockPath = path.string() + ".lock";
  const int held = ::open(lockPath.c_str(), O_RDWR);
  assert(held >= 0 && ::flock(held, LOCK_EX) == 0);
  assert(pipe(gate) == 0 && pipe(results) == 0);
  child = fork();
  assert(child >= 0);
  if (child == 0) {
    close(held); close(gate[0]); close(results[0]);
    assert(::write(gate[1], "x", 1) == 1);
    const auto value = ClientIdentity::loadOrCreate();
    assert(value == common);
    assert(::write(results[1], value.data(), value.size()) == 36);
    _exit(0);
  }
  close(gate[1]); close(results[1]);
  char begin;
  readExactly(gate[0], &begin, 1);
  close(gate[0]);
  pollfd awaiting{results[0], POLLIN, 0};
  assert(::poll(&awaiting, 1, 50) == 0);
  assert(::flock(held, LOCK_UN) == 0);
  close(held);
  std::string final(36, '\0');
  readExactly(results[0], final.data(), final.size());
  close(results[0]);
  waitSuccessful(child);
  assert(final == common && fs::is_regular_file(lockPath));
  assert(ClientIdentity::loadOrCreate() == common);

  if (previous) assert(setenv("HOME", previousHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
  std::cout << "ClientIdentityIntegrityTests passed\n";
}
