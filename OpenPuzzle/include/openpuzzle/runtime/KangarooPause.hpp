#pragma once

// Linux assignment pause protocol. Native checkpoint bytes are never rewritten.
#ifndef _WIN32
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace openpuzzle {
class KangarooPause {
  using State = client::ClientExecutionState;
  struct Record { std::string phase, binding, boot; int owner = 0; std::uint64_t start = 0; };
  static std::filesystem::path path(const State& s) {
    return std::filesystem::path(s.workspace) / "kangaroo.pause";
  }
  static std::string binding(const State& s) {
    std::ostringstream out;
    out << std::quoted(s.assignmentId) << ' ' << std::quoted(s.clientId) << ' '
        << s.pid << ' ' << std::quoted(s.bootId) << ' ' << s.processStartTime << ' '
        << s.device << ' ' << s.kangarooGeneration << ' ' << std::quoted(s.kangarooWalkSeed)
        << ' ' << std::quoted(s.start) << ' ' << std::quoted(s.end) << ' '
        << std::quoted(s.publicKey);
    return out.str();
  }
  static void privateDirectory(const State& s) {
    struct stat st{};
    if (::lstat(s.workspace.c_str(), &st) || !S_ISDIR(st.st_mode) ||
        st.st_uid != ::getuid() || (st.st_mode & 0077))
      throw std::runtime_error("Kangaroo pause workspace is not private and direct");
  }
  static std::optional<Record> read(const State& s) {
    privateDirectory(s);
    const int fd = ::open(path(s).c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) {
      if (errno == ENOENT) return {};
      throw std::runtime_error("Kangaroo pause intent is unreadable; state preserved");
    }
    struct stat st{};
    char bytes[8192];
    const bool safe = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
        st.st_uid == ::getuid() && !(st.st_mode & 0077) && st.st_size > 0 && st.st_size < 8192;
    const auto size = safe ? ::read(fd, bytes, sizeof(bytes)) : -1;
    ::close(fd);
    Record r; std::string magic, extra;
    std::istringstream in(size > 0 ? std::string(bytes, size) : "");
    if (!safe || size != st.st_size || !(in >> magic >> r.phase >> std::quoted(r.binding)
        >> r.owner >> std::quoted(r.boot) >> r.start) || (in >> extra) ||
        magic != "OP-KANGAROO-PAUSE-1" || (r.phase != "requested" && r.phase != "paused") ||
        r.binding != binding(s) || r.owner <= 0 || r.boot.empty() || r.start == 0)
      throw std::runtime_error("Kangaroo pause intent differs; state preserved");
    return r;
  }
  static bool write(const State& s, const Record& r) {
    std::ostringstream out;
    out << "OP-KANGAROO-PAUSE-1\n" << r.phase << '\n' << std::quoted(r.binding)
        << '\n' << r.owner << '\n' << std::quoted(r.boot) << '\n' << r.start << '\n';
    return writePrivateFileAtomically(path(s), out.str());
  }
  static bool gone(int pid, std::uint64_t start) {
    const auto current = LinuxProcessIdentity::startTime(pid);
    if (current && *current != start) return true;
    if (!current) return ::kill(pid, 0) != 0 && errno == ESRCH;
    std::ifstream input("/proc/" + std::to_string(pid) + "/stat");
    std::string line; std::getline(input, line);
    const auto close = line.rfind(')');
    return close != std::string::npos && close + 2 < line.size() && line[close + 2] == 'Z';
  }
  static void descendants(int parent, std::vector<int>& found, int depth = 0) {
    if (depth > 12 || found.size() > 128) throw std::runtime_error("Kangaroo process tree is ambiguous");
    std::ifstream children("/proc/" + std::to_string(parent) + "/task/" + std::to_string(parent) + "/children");
    int child;
    while (children >> child) { found.push_back(child); descendants(child, found, depth + 1); }
  }
public:
  static bool supported(const State& s) {
    return (s.engine == "Kangaroo" || s.engine == "PSCKangaroo" || s.engine == "kangaroo" ||
            s.engine == "psckangaroo") && (s.backend == "CUDA" || s.backend == "cuda");
  }
  static bool present(const State& s) {
    if (!supported(s)) return false;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path(s), error);
    return error ? error != std::errc::no_such_file_or_directory :
        status.type() != std::filesystem::file_type::not_found;
  }
  static bool paused(const State& s) {
    if (!present(s)) return false;
    const auto r = read(s); return r && r->phase == "paused";
  }
  static bool requestedByThisRuntime(const State& s) {
    if (!present(s)) return false;
    const auto r = read(s);
    return r && r->owner == ::getpid() && r->boot == client::ClientStateStore::currentBootId() &&
        LinuxProcessIdentity::startTime(::getpid()) == r->start;
  }
  static bool request(const State& s, const std::string& slot) {
    if (!supported(s) || !s.valid() || !ClientRuntimeControl::running(slot)) return false;
    const auto owner = ClientRuntimeControl::runtimePid(slot);
    const auto boot = ClientRuntimeControl::runtimeBootId(slot);
    const auto start = ClientRuntimeControl::runtimeStartTime(slot);
    if (!owner || !boot || !start) return false;
    if (const auto old = read(s))
      return old->owner == *owner && old->boot == *boot && old->start == *start;
    const Record r{"requested", binding(s), *boot, *owner, *start};
    return write(s, r);
  }
  static bool clear(const State& s) {
    if (!read(s)) return true;
    return ::unlink(path(s).c_str()) == 0;
  }
  static bool checkpointReady(const State& s) {
    const auto checkpoint = std::filesystem::path(s.workspace) / "kangaroo.checkpoint";
    const int fd = ::open(checkpoint.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) return false;
    struct stat st{}; char magic[8]{};
    const bool ok = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == ::getuid() &&
        !(st.st_mode & 0077) && st.st_size > 104 && ::read(fd, magic, 8) == 8 &&
        (std::string(magic, 7) == "OPZKCP1" || std::string(magic, 6) == "RCKDT5");
    ::close(fd); return ok;
  }
  // Signal only the identified native child. Its shell supervisor stays alive
  // until saving finishes. Never escalate to SIGKILL during checkpoint writing.
  static bool stopAndSave(const State& s, const std::filesystem::path& executable,
                          std::chrono::seconds timeout = std::chrono::minutes(10)) {
    auto record = read(s);
    if (!record || !requestedByThisRuntime(s) || s.bootId != client::ClientStateStore::currentBootId() ||
        LinuxProcessIdentity::startTime(s.pid) != s.processStartTime) return false;
    std::error_code error;
    const auto expected = std::filesystem::canonical(executable, error);
    if (error) return false;
    std::vector<int> children; descendants(s.pid, children);
    int native = 0; std::uint64_t nativeStart = 0;
    for (int pid : children) {
      const auto start = LinuxProcessIdentity::startTime(pid);
      const auto binary = std::filesystem::read_symlink("/proc/" + std::to_string(pid) + "/exe", error);
      if (!error && binary == expected && start) {
        if (native) return false;
        native = pid; nativeStart = *start;
      }
      error.clear();
    }
    if (!native) return false;
    const auto log = std::filesystem::path(s.workspace) / "kangaroo.log";
    const auto offset = std::filesystem::file_size(log, error);
    if (error || !LinuxProcessIdentity::signalIfMatches(native, nativeStart, SIGTERM)) return false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!gone(native, nativeStart) || !gone(s.pid, s.processStartTime)) {
      if (std::chrono::steady_clock::now() >= deadline) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::ifstream input(log); input.seekg(offset);
    std::string tail((std::istreambuf_iterator<char>(input)), {});
    if (tail.find("Checkpoint saved successfully.") == std::string::npos || !checkpointReady(s)) return false;
    record->phase = "paused";
    return write(s, *record);
  }
  static bool archiveExit(const State& s) {
    if (!paused(s) || !checkpointReady(s)) return false;
    const auto exit = std::filesystem::path(s.workspace) / "exit.code";
    struct stat st{};
    if (::lstat(exit.c_str(), &st)) return errno == ENOENT;
    if (!S_ISREG(st.st_mode) || st.st_uid != ::getuid() || (st.st_mode & 0077)) return false;
    const auto archive = std::filesystem::path(s.workspace) /
        ("pause-exit-" + std::to_string(s.pid) + "-" + std::to_string(s.processStartTime) + ".code");
    if (::link(exit.c_str(), archive.c_str())) return false;
    return ::unlink(exit.c_str()) == 0;
  }
};
} // namespace openpuzzle
#endif
