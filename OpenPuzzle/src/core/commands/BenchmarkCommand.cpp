#include "openpuzzle/core/CliInteger.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/PosixProcessRunner.hpp"
#include "openpuzzle/core/SignalHandler.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/hardware/RusticlEnvironment.hpp"
#include "openpuzzle/performance/AutoTuner.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/runtime/VanityRangeProtocol.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace openpuzzle {

static bool hasArg(const std::vector<std::string> &args,
                   const std::string &name) {
  for (const auto &arg : args) {
    if (arg == name)
      return true;
  }

  return false;
}


static std::string getStringArg(
    const std::vector<std::string> &args,
    const std::string &name,
    const std::string &fallback) {
  for (
      std::size_t index = 0;
      index + 1 < args.size();
      ++index) {
    if (args[index] == name) {
      return args[index + 1];
    }
  }

  return fallback;
}

static std::runtime_error benchmarkError(
    const std::string &code,
    const std::string &problem,
    const std::string &action) {
  return std::runtime_error(
      "OpenPuzzle benchmark failed\n"
      "---------------------------\n"
      "Error code......... " + code + "\n"
      "Problem............ " + problem + "\n"
      "Action............. " + action);
}

static std::string normalizeBackend(
    std::string backend) {
  std::transform(
      backend.begin(),
      backend.end(),
      backend.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  return backend;
}

namespace {
// One local synthetic workspace must not be shared by simultaneous benchmarks.
class BenchmarkLock {
  int fd_ = -1;
public:
  explicit BenchmarkLock(const fs::path& workspace) {
    fd_ = ::open((workspace / "benchmark.lock").c_str(),
                 O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    struct stat metadata{};
    if (fd_ < 0 || fstat(fd_, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        metadata.st_uid != geteuid() || (metadata.st_mode & 0077) != 0 ||
        flock(fd_, LOCK_EX | LOCK_NB) != 0) {
      if (fd_ >= 0) ::close(fd_);
      throw std::runtime_error("Another benchmark owns the workspace, or its lock is unsafe.");
    }
  }
  ~BenchmarkLock() { if (fd_ >= 0) ::close(fd_); }
  BenchmarkLock(const BenchmarkLock&) = delete;
  BenchmarkLock& operator=(const BenchmarkLock&) = delete;
};
void requireBenchmarkIdle() {
  client::ExecutionSyncService sync;
  for (const auto& slot : ClientRuntimeControl::executionSlots()) {
    const auto execution = sync.inspect(slot);
    if ((execution.hasState && (execution.running || execution.identityUnavailable)) ||
        ClientRuntimeControl::runtimeIdentityStatus(slot) !=
            ClientRuntimeControl::RuntimeIdentityStatus::Inactive) {
      throw std::runtime_error("Benchmark refused: stop active searches and resolve unavailable process identity first.");
    }
  }
}
int cancelledBenchmark() {
  std::cout << "Benchmark cancelled. Previous GPU profile preserved.\n";
  return 130;
}
struct FlushBenchmarkOutput {
  std::ios_base::fmtflags flags = std::cout.flags();
  FlushBenchmarkOutput() { std::cout << std::unitbuf; }
  ~FlushBenchmarkOutput() { std::cout.flags(flags); }
};

std::string benchmarkShellQuote(const std::string& value) {
  if (value.find('\0') != std::string::npos) {
    throw std::runtime_error("NUL in benchmark argument");
  }
  std::string result = "'";
  for (const char character : value) {
    result += character == '\'' ? "'\\''" : std::string(1, character);
  }
  return result + "'";
}

std::vector<BenchmarkConfiguration> vanityCandidateMatrix(int computeUnits) {
  int anchor = 64;
  const int requested = std::max(1, computeUnits) * 2;
  while (anchor < requested && anchor < 4096) anchor *= 2;
  anchor = std::clamp(anchor, 64, 4096);

  std::vector<BenchmarkConfiguration> result;
  for (const int factor : {1, 2, 4, 8, 16}) {
    const auto blocks = static_cast<long long>(anchor) * factor;
    if (blocks > 4096) break;
    result.push_back({static_cast<int>(blocks), 256, 1024});
  }
  while (result.size() < 5 && result.front().blocks > 64) {
    result.insert(result.begin(), {result.front().blocks / 2, 256, 1024});
  }
  return result;
}

BenchmarkResult runVanityCandidate(
    const std::string& engine,
    const fs::path& workspace,
    const BenchmarkConfiguration& configuration,
    int seconds) {
  using namespace vanityPreview;
  BenchmarkResult result;
  result.configuration = configuration;
  std::string diagnostic;
  int nativeExit = -1;
  const auto reject = [&](const std::string& reason) {
    std::cerr << "Vanity candidate... " << reason << "\n";
    if (nativeExit >= 0) std::cerr << "Native exit........ " << nativeExit << "\n";
    if (!diagnostic.empty()) std::cerr << "Native diagnostic.. " << diagnostic << "\n";
    return result;
  };
  if (configuration.blocks < 64 || configuration.blocks > 4096 ||
      (configuration.blocks & (configuration.blocks - 1)) ||
      configuration.threads != 256 || configuration.points != 1024) {
    return reject("unsupported grid/threads/points");
  }

  const std::string start = "10000000000000000";
  const std::uint64_t count = std::uint64_t{1} << 40;
  Protocol protocol(Plan(start, count, configuration.blocks));
  const auto targets = workspace / "vanity-benchmark-targets.txt";
  const auto output = workspace / "vanity-benchmark-found.txt";
  const std::string targetText =
      "OPV-TARGETS-1\np2pkh 1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH\n";

  if (fs::exists(targets) || fs::is_symlink(targets)) {
    if (fs::is_symlink(targets) || !fs::is_regular_file(targets)) return reject("unsafe benchmark target file");
    std::ifstream input(targets);
    if (std::string((std::istreambuf_iterator<char>(input)), {}) != targetText) return reject("benchmark target contents differ; preserved");
  } else if (!writePrivateFileAtomically(targets, targetText)) {
    return reject("cannot create private benchmark target file");
  }
  if (fs::exists(output) || fs::is_symlink(output)) {
    if (fs::is_symlink(output) || !fs::is_regular_file(output) || !fs::remove(output)) return reject("unsafe or occupied benchmark output");
  }

  std::ostringstream command;
  command << "exec"; // Keep the native engine directly owned by this client.
  for (const auto& argument : std::vector<std::string>{
           engine, "--target-file", start, std::to_string(count),
           std::to_string(configuration.blocks), targets.string(),
           output.string(), "exhaust", std::to_string(getpid())}) {
    if (command.tellp() > 0) command << ' ';
    command << benchmarkShellQuote(argument);
  }

  bool invalid = false;
  std::vector<double> samples;
  std::uint64_t previous = 0;
  auto previousTime = std::chrono::steady_clock::now();
  PosixProcessRunner runner;
  const auto execution = runner.run(
      command.str(),
      [&](const std::string& line) {
        if (!line.starts_with("OPV1 ")) {
          if (line.starts_with("FAILED: ") || diagnostic.empty()) diagnostic = line.substr(0, 512);
          return;
        }
        if (invalid) return;
        try {
          protocol.consume(line);
          if (!line.starts_with("OPV1 PROGRESS ")) return;
          const auto now = std::chrono::steady_clock::now();
          const double elapsed = std::chrono::duration<double>(now - previousTime).count();
          if (elapsed > 0.0 && protocol.checked > previous) {
            samples.push_back(
                static_cast<double>(protocol.checked - previous) / elapsed / 1000000.0);
          }
          previous = protocol.checked;
          previousTime = now;
        } catch (const std::exception& error) {
          invalid = true;
          diagnostic = (std::string("Protocol error: ") + error.what()).substr(0, 512);
        }
      },
      seconds,
      [] { return SignalHandler::stopRequested(); });

  nativeExit = execution.exitCode;
  if (SignalHandler::stopRequested()) return result;
  if (!execution.started || invalid ||
      !protocol.begun || (!protocol.stopped && !protocol.complete) ||
      !fs::is_regular_file(output) || fs::file_size(output) != 0) {
    return reject("native session did not finish coherently");
  }
  const std::size_t first = samples.size() > 2 ? 2 : samples.size();
  if (samples.size() - first < 4) return reject("fewer than four usable progress samples");
  double total = 0.0;
  result.minimumSpeed = std::numeric_limits<double>::max();
  for (std::size_t index = first; index < samples.size(); ++index) {
    const double value = samples[index];
    if (!std::isfinite(value) || value <= 0.0) return reject("invalid native speed sample");
    total += value;
    result.minimumSpeed = std::min(result.minimumSpeed, value);
    result.maximumSpeed = std::max(result.maximumSpeed, value);
  }
  result.samples = static_cast<int>(samples.size() - first);
  result.averageSpeed = total / result.samples;
  result.speedMKeys = result.averageSpeed;
  result.success = std::isfinite(result.averageSpeed) && result.averageSpeed > 0.0;
  return result;
}
} // namespace

int BenchmarkCommand::run(const std::vector<std::string> &args) const {
  SignalHandler::install();
  FlushBenchmarkOutput flushOutput;
  const auto requireIdle = [&] {
    if (hasArg(args, "--require-idle")) requireBenchmarkIdle();
  };

  // Validate every numeric option before any context or Rusticl mutation.
  try {
    (void) cliIntegerArgument(args, {"--gpu", "--d"}, 0);
    (void) cliIntegerArgument(args, {"--seconds"}, 30);
    (void) cliIntegerArgument(args, {"--samples"}, 8);
    (void) cliIntegerArgument(args, {"--blocks", "--b"}, 256);
    (void) cliIntegerArgument(args, {"--threads", "--t"}, 256);
    (void) cliIntegerArgument(args, {"--points", "--p"}, 256);
  } catch (const std::exception& error) {
    throw benchmarkError("OP-BENCH-011", error.what(),
        "supply each numeric option once, with a complete integer value");
  }

  const auto loaded = ConfigurationManager::loadChecked();
  if (!loaded) {
    throw benchmarkError(
        "OP-BENCH-010",
        "configuration is unreadable or invalid; original file preserved",
        "check ~/.config/OpenPuzzle/config.json before retrying");
  }
  auto configuration = *loaded;
  int gpu = cliIntegerArgument(args, {"--gpu", "--d"}, configuration.gpu.device);

  const std::string configuredBackend =
      configuration.engine.backend.empty()
          ? "cuda"
          : configuration.engine.backend;

  const std::string backend =
      normalizeBackend(
          getStringArg(
              args,
              "--backend",
              configuredBackend));

  const std::string requestedEngine =
      normalizeBackend(getStringArg(args, "--engine", "bitcrack"));

  if (requestedEngine != "bitcrack" && requestedEngine != "vanity") {
    throw benchmarkError(
        "OP-BENCH-012",
        "unsupported benchmark engine: " + requestedEngine,
        "use --engine bitcrack or --engine vanity");
  }

  if (
      backend != "cuda" &&
      backend != "opencl") {
    throw benchmarkError(
        "OP-BENCH-003",
        "unsupported BitCrack backend: " + backend,
        "use --backend cuda or --backend opencl");
  }

  if (requestedEngine == "vanity" && backend != "cuda") {
    throw benchmarkError(
        "OP-BENCH-013",
        "Vanity automatic tuning requires CUDA",
        "use --engine vanity --backend cuda --auto --real");
  }

  if (gpu < 0) {
    throw benchmarkError(
        "OP-BENCH-004",
        "GPU device must not be negative",
        "use --gpu followed by a non-negative device number");
  }

  const bool hasRusticlSelector =
      hasArg(args, "--rusticl-enable");

  const std::string rusticlSelector =
      getStringArg(
          args,
          "--rusticl-enable",
          configuration.gpu.rusticlEnable);

  if (hasRusticlSelector && backend != "opencl") {
    throw benchmarkError(
        "OP-BENCH-005",
        "--rusticl-enable requires the OpenCL backend",
        "add --backend opencl or remove --rusticl-enable");
  }

  if (!ToolManager::supportsBackend(backend)) {
    throw benchmarkError(
        "OP-ENGINE-002",
        "this package does not support the " + backend + " backend",
        "run: openpuzzle doctor");
  }

  const std::string backendLabel =
      backend == "opencl"
          ? "OpenCL"
          : "CUDA";

  bool real = hasArg(args, "--real");
  bool autoMode = hasArg(args, "--auto");

  int seconds = cliIntegerArgument(args, {"--seconds"}, 30);
  int samples = cliIntegerArgument(args, {"--samples"}, 8);
  int blocks = cliIntegerArgument(args, {"--blocks", "--b"}, 256);
  int threads = cliIntegerArgument(args, {"--threads", "--t"}, 256);
  int points = cliIntegerArgument(args, {"--points", "--p"}, 256);

  if (seconds < 10 || seconds > 300) {
    throw benchmarkError(
        "OP-BENCH-006",
        "duration must be between 10 and 300 seconds",
        "use --seconds with a value from 10 to 300");
  }

  if (samples < 4 || samples > 60) {
    throw benchmarkError(
        "OP-BENCH-007",
        "samples must be between 4 and 60",
        "use --samples with a value from 4 to 60");
  }

  if (blocks <= 0 || threads <= 0 || points <= 0) {
    throw benchmarkError(
        "OP-BENCH-008",
        "launch parameters must be positive",
        "use positive --blocks, --threads and --points values");
  }

  if (SignalHandler::stopRequested()) return cancelledBenchmark();
  if (real) requireIdle();

  CommandContext context;

  if (!context.initialize(backend)) {
    std::cerr
        << "OpenPuzzle benchmark failed\n"
        << "---------------------------\n"
        << "Error code......... OP-BENCH-000\n"
        << "Problem............ "
        << context.lastError()
        << '\n'
        << "Action............. run: openpuzzle doctor\n";
    return 1;
  }

  if (backend == "opencl" && !rusticlSelector.empty()) {
    RusticlEnvironment::apply(rusticlSelector);
  }

  if (hasRusticlSelector &&
      configuration.gpu.rusticlEnable != rusticlSelector) {
    configuration.gpu.rusticlEnable = rusticlSelector;
    if (!ConfigurationManager::save(configuration)) {
      throw benchmarkError(
          "OP-BENCH-009",
          "unable to persist the Rusticl selector",
          "check ownership and write permissions for ~/.config/OpenPuzzle");
    }
  }

  std::cout << "====================================\n";
  std::cout << "      OpenPuzzle GPU Benchmark\n";
  std::cout << "====================================\n\n";
  const auto gpuInfo =
      GpuManager::currentGpu(
          backendLabel,
          gpu);

  std::cout << "Backend......... "
            << backendLabel
            << "\n";
  std::cout << "Engine.......... "
            << (requestedEngine == "vanity" ? "Vanity" : "BitCrack")
            << "\n";
  std::cout << "GPU............. " << gpu << "\n";
  std::cout << "GPU name........ " << gpuInfo.name << "\n";
  std::cout << "Compute units... " << gpuInfo.computeUnits << "\n";
  std::cout << "Memory.......... " << gpuInfo.memoryMb << " MiB\n";
  std::cout << "Maximum seconds. " << seconds << "\n";
  std::cout << "Target samples.. " << samples << "\n";
  std::cout << "Warm-up samples. 2\n\n";

  if (!real) {
    std::cout << "Use --real to run a real benchmark.\n";
    (void)autoMode;
    return 0;
  }

  if (requestedEngine == "vanity") {
    if (!autoMode) {
      throw benchmarkError(
          "OP-BENCH-014",
          "Vanity requires automatic geometry selection",
          "add --auto");
    }
    const auto vanity = ToolManager::vanityPath();
    if (!vanity || !fs::is_regular_file(*vanity)) {
      throw benchmarkError(
          "OP-ENGINE-006",
          "bundled Vanity CUDA executable is unavailable",
          "reinstall OpenPuzzle and run: openpuzzle doctor --offline");
    }
    std::string identity;
    const auto identityResult = PosixProcessRunner().run(
        benchmarkShellQuote(*vanity) + " --identity",
        [&](const std::string& line) { identity += line + "\n"; }, 20);
    if (!identityResult.started || identityResult.exitCode != 0 ||
        identity != "OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1\n") {
      throw benchmarkError(
          "OP-ENGINE-007",
          "bundled Vanity identity was rejected",
          "reinstall OpenPuzzle and run: openpuzzle doctor --offline");
    }
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
      throw benchmarkError("OP-ENV-001", "HOME is not available", "run from a normal user login session");
    }
    const auto workspace = fs::path(home) / ".local/share/OpenPuzzle/benchmark-vanity";
    WorkspaceSecurity::prepare(workspace);
    BenchmarkLock benchmarkLock(workspace);
    const auto visibleDevice = gpuInfo.uuid.empty() ? std::to_string(gpu) : gpuInfo.uuid;
    if (setenv("CUDA_DEVICE_ORDER", "PCI_BUS_ID", 1) != 0 ||
        setenv("CUDA_VISIBLE_DEVICES", visibleDevice.c_str(), 1) != 0) {
      throw benchmarkError("OP-BENCH-015", "cannot isolate the selected CUDA device", "check the CUDA environment");
    }

    AutoTuner tuner;
    std::vector<BenchmarkResult> results;
    const auto candidates = vanityCandidateMatrix(gpuInfo.computeUnits);
    std::cout << "Candidate matrix. " << candidates.size() << " configurations\n";
    int index = 1;
    for (const auto& candidate : candidates) {
      if (SignalHandler::stopRequested()) return cancelledBenchmark();
      requireIdle();
      std::cout << "\n[" << index++ << "/" << candidates.size() << "] "
                << "grid=" << candidate.blocks << " threads=256 points=1024\n";
      auto measured = runVanityCandidate(*vanity, workspace, candidate, seconds);
      results.push_back(measured);
      std::cout << "Average.......... " << measured.averageSpeed << " MKey/s\n"
                << "Minimum.......... " << measured.minimumSpeed << " MKey/s\n"
                << "Maximum.......... " << measured.maximumSpeed << " MKey/s\n"
                << "Samples.......... " << measured.samples << "\n"
                << "Success.......... " << (measured.success ? "yes" : "no") << "\n";
    }
    auto best = tuner.selectBest(results);
    if (!best.success) {
      std::cerr << "\nNo valid Vanity benchmark configuration was found.\n";
      return 1;
    }
    const int validationSeconds = std::min(120, std::max(20, seconds * 2));
    std::cout << "\nValidating Vanity finalist\n"
              << "---------------------------\n"
              << "Grid............. " << best.configuration.blocks << "\n"
              << "Maximum seconds.. " << validationSeconds << "\n";
    requireIdle();
    auto validated = runVanityCandidate(*vanity, workspace, best.configuration, validationSeconds);
    if (SignalHandler::stopRequested()) return cancelledBenchmark();
    if (!validated.success) {
      std::cerr << "Final validation.. failed\nGPU profile was not changed.\n";
      return 1;
    }
    const double planningSpeed = AutoTuner::planningSpeed(validated);
    GpuProfileRecord profile;
    profile.gpuName = gpuInfo.name;
    profile.backend = "CUDA";
    profile.engine = "Vanity";
    profile.blocks = validated.configuration.blocks;
    profile.threads = 256;
    profile.points = 1024;
    profile.averageSpeed = planningSpeed;
    profile.minimumSpeed = validated.minimumSpeed;
    profile.maximumSpeed = validated.maximumSpeed;
    profile.samples = validated.samples;
    if (SignalHandler::stopRequested()) return cancelledBenchmark();
    requireIdle();
    const bool saved = GpuProfileManager(context.db).save(profile);
    std::cout << "\nValidated Vanity configuration\n"
              << "------------------------------\n"
              << "Grid............. " << profile.blocks << "\n"
              << "Threads.......... " << profile.threads << "\n"
              << "Points........... " << profile.points << "\n"
              << "Average.......... " << validated.averageSpeed << " MKey/s\n"
              << "Planning speed... " << planningSpeed << " MKey/s\n"
              << "Saved profile..... " << (saved ? "yes" : "no") << "\n";
    return saved ? 0 : 1;
  }

  const auto bitcrack =
      backend == "opencl"
          ? ToolManager::bitcrackOpenCLPath()
          : ToolManager::bitcrackCudaPath();

  if (!bitcrack) {
    throw benchmarkError(
        "OP-ENGINE-003",
        "BitCrack " + backendLabel + " executable is not configured",
        "run: openpuzzle doctor");
  }

  if (
      !fs::is_regular_file(
          *bitcrack)) {
    throw benchmarkError(
        "OP-ENGINE-004",
        "BitCrack " + backendLabel + " executable was not found: " + *bitcrack,
        "reinstall OpenPuzzle and run: openpuzzle doctor");
  }

  /*
   * A public client starts with an empty local
   * database. Benchmark execution must therefore
   * never depend on puzzle, job or range records.
   *
   * The synthetic target has private key 1 while
   * the benchmark starts at key 2. Consequently,
   * no private key can be produced.
   */
  Database &db = context.db;

  PuzzleRecord benchmarkPuzzle{};
  benchmarkPuzzle.address =
      "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";

  RangeRecord benchmarkRange{};
  benchmarkRange.startKey = "2";

  /*
   * A large synthetic interval prevents BitCrack
   * from reaching the end during benchmarking.
   */
  benchmarkRange.endKey =
      "FFFFFFFFFFFFFFFFFF";

  Scheduler &scheduler = context.scheduler;

  const char *home =
      std::getenv("HOME");

  if (
      home == nullptr ||
      *home == '\0') {
    throw benchmarkError(
        "OP-ENV-001",
        "HOME is not available for the benchmark workspace",
        "run OpenPuzzle from a normal user login session");
  }

  const auto workspace =
      fs::path(home) /
      ".local" /
      "share" /
      "OpenPuzzle" /
      "benchmark";

  WorkspaceSecurity::prepare(
      workspace);

  const auto output =
      (workspace /
       "benchmark-safe-found.txt")
          .string();

  /*
   * This file belongs only to the synthetic target
   * above and can never contain a real puzzle key.
   */
  BenchmarkLock benchmarkLock(workspace);
  if (SignalHandler::stopRequested()) return cancelledBenchmark();
  fs::remove(output);
  fs::remove(
      workspace /
      "stdout.log");

  BenchmarkConfiguration cfg;
  cfg.blocks = blocks;
  cfg.threads = threads;
  cfg.points = points;

  auto command = scheduler.buildBitCrackCommand(bitcrack.value(), benchmarkPuzzle, benchmarkRange, gpu,
                                                cfg.blocks, cfg.threads,
                                                cfg.points, output);

  ExecutionContext ctx;
  ctx.executionId = 0;
  ctx.puzzleId = 0;
  ctx.jobId = 0;
  ctx.rangeId = 0;
  ctx.engine = "BitCrack";
  ctx.workspace =
      workspace.string();
  ctx.command = command;
  /*
   * Never echo raw engine output. Benchmark output
   * is performance data only.
   */
  ctx.echoOutput = false;
  ctx.stopRequested = [] { return SignalHandler::stopRequested(); };

  BenchmarkRunner runner;

  if (autoMode) {
    AutoTuner tuner;

    const auto candidates =
        tuner.defaultMatrix(
            gpuInfo.computeUnits,
            gpuInfo.memoryMb,
            backend == "cuda");

    std::vector<BenchmarkResult> results;

    std::cout
        << "Candidate matrix. "
        << candidates.size()
        << " configurations\n";

    int index = 1;

    for (const auto &candidate :
         candidates) {
      if (SignalHandler::stopRequested()) return cancelledBenchmark();
      requireIdle();
      const auto &configuration =
          candidate.configuration;

      const auto configCommand =
          scheduler.buildBitCrackCommand(
              bitcrack.value(),
              benchmarkPuzzle,
              benchmarkRange,
              gpu,
              configuration.blocks,
              configuration.threads,
              configuration.points,
              output);

      ctx.command = configCommand;

      std::cout
          << "\n["
          << index
          << "/"
          << candidates.size()
          << "] "
          << "b="
          << configuration.blocks
          << " t="
          << configuration.threads
          << " p="
          << configuration.points
          << " memory~"
          << AutoTuner::estimatedMemoryMb(
                 configuration)
          << " MiB\n";

      const auto item =
          runner.run(
              configuration,
              ctx,
              seconds,
              samples);

      if (SignalHandler::stopRequested()) return cancelledBenchmark();
      results.push_back(item);

      std::cout
          << "Average.......... "
          << item.averageSpeed
          << " MKey/s\n"
          << "Minimum.......... "
          << item.minimumSpeed
          << " MKey/s\n"
          << "Maximum.......... "
          << item.maximumSpeed
          << " MKey/s\n"
          << "Samples.......... "
          << item.samples
          << "\n"
          << "Success.......... "
          << (
                 item.success
                     ? "yes"
                     : "no")
          << "\n";

      ++index;
    }

    auto best =
        tuner.selectBest(results);

    if (!best.success) {
      std::cerr
          << "\nNo valid benchmark "
             "configuration was found.\n";

      return 1;
    }

    const int validationSeconds =
        std::min(
            300,
            std::max(
                60,
                seconds * 2));

    const int validationSamples =
        std::min(
            60,
            std::max(
                16,
                samples * 2));

    std::cout
        << "\nValidating finalist\n"
        << "-------------------\n"
        << "Blocks........... "
        << best.configuration.blocks
        << "\n"
        << "Threads.......... "
        << best.configuration.threads
        << "\n"
        << "Points........... "
        << best.configuration.points
        << "\n"
        << "Maximum seconds.. "
        << validationSeconds
        << "\n"
        << "Target samples... "
        << validationSamples
        << "\n";

    ctx.command =
        scheduler.buildBitCrackCommand(
            bitcrack.value(),
            benchmarkPuzzle,
            benchmarkRange,
            gpu,
            best.configuration.blocks,
            best.configuration.threads,
            best.configuration.points,
            output);

    if (SignalHandler::stopRequested()) return cancelledBenchmark();
    requireIdle();
    const auto validated =
        runner.run(
            best.configuration,
            ctx,
            validationSeconds,
            validationSamples);

    if (SignalHandler::stopRequested()) return cancelledBenchmark();
    if (!validated.success) {
      std::cerr
          << "Final validation.. failed\n"
          << "GPU profile was not changed.\n";

      return 1;
    }

    best = validated;

    const double planningSpeed =
        AutoTuner::planningSpeed(best);

    std::cout
        << "\nValidated configuration\n"
        << "-----------------------\n"
        << "Blocks........... "
        << best.configuration.blocks
        << "\n"
        << "Threads.......... "
        << best.configuration.threads
        << "\n"
        << "Points........... "
        << best.configuration.points
        << "\n"
        << "Average.......... "
        << best.averageSpeed
        << " MKey/s\n"
        << "Minimum.......... "
        << best.minimumSpeed
        << " MKey/s\n"
        << "Maximum.......... "
        << best.maximumSpeed
        << " MKey/s\n"
        << "Samples.......... "
        << best.samples
        << "\n"
        << "Planning speed... "
        << planningSpeed
        << " MKey/s\n";

    GpuProfileRecord profile;

    profile.gpuName =
        gpuInfo.name;
    profile.backend =
        backendLabel;
    profile.engine = "BitCrack";
    profile.blocks =
        best.configuration.blocks;
    profile.threads =
        best.configuration.threads;
    profile.points =
        best.configuration.points;
    profile.averageSpeed =
        planningSpeed;
    profile.minimumSpeed =
        best.minimumSpeed;
    profile.maximumSpeed =
        best.maximumSpeed;
    profile.samples =
        best.samples;

    GpuProfileManager profileManager(db);

    if (SignalHandler::stopRequested()) return cancelledBenchmark();
    requireIdle();
    const bool saved =
        profileManager.save(profile);

    std::cout
        << "Saved profile..... "
        << (
               saved
                   ? "yes"
                   : "no")
        << "\n";

    return saved
               ? 0
               : 1;
  }

  auto result = runner.run(cfg, ctx, seconds, samples);
  if (SignalHandler::stopRequested()) return cancelledBenchmark();

  std::cout << "\nBenchmark result\n\n";
  std::cout << "Blocks........... " << result.configuration.blocks << "\n";
  std::cout << "Threads.......... " << result.configuration.threads << "\n";
  std::cout << "Points........... " << result.configuration.points << "\n";
  std::cout << "Average.......... " << result.averageSpeed << " MKey/s\n";
  std::cout << "Minimum.......... " << result.minimumSpeed << " MKey/s\n";
  std::cout << "Maximum.......... " << result.maximumSpeed << " MKey/s\n";
  std::cout << "Samples.......... " << result.samples << "\n";
  std::cout << "Success.......... " << (result.success ? "yes" : "no") << "\n";

  return result.speedMKeys > 0.0 ? 0 : 1;
}

} // namespace openpuzzle
