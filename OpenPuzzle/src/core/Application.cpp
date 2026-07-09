#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"
#include "openpuzzle/allocator/RangeAllocator.hpp"
#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionSession.hpp"
#include "openpuzzle/core/ProcessRunner.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/core/commands/BenchmarkCommand.hpp"
#include "openpuzzle/core/commands/DispatchCommand.hpp"
#include "openpuzzle/core/commands/WorkerRunCommand.hpp"
#include "openpuzzle/core/commands/ProfileCommand.hpp"
#include "openpuzzle/core/commands/StartJobCommand.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/AutoTuner.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include "openpuzzle/services/PuzzleService.hpp"
#include "openpuzzle/services/WorkerService.hpp"
#include "openpuzzle/services/QueueService.hpp"
#include "openpuzzle/services/BenchmarkService.hpp"
#include "openpuzzle/services/EngineService.hpp"
#include "openpuzzle/services/ExecutionService.hpp"
#include "openpuzzle/services/DoctorService.hpp"
#include "openpuzzle/services/DashboardService.hpp"
#include "openpuzzle/services/DaemonService.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/wait.h>
#ifndef WEXITSTATUS
#define WEXITSTATUS(x) (x)
#endif
namespace fs = std::filesystem;
namespace openpuzzle {
static std::string st(RangeStatus s) {
  switch (s) {
  case RangeStatus::Reserved:
    return "RESERVED";
  case RangeStatus::Running:
    return "RUNNING";
  case RangeStatus::Completed:
    return "COMPLETED";
  case RangeStatus::Failed:
    return "FAILED";
  case RangeStatus::Cancelled:
    return "CANCELLED";
  case RangeStatus::External:
    return "EXTERNAL";
  }
  return "UNKNOWN";
}
std::string Application::dbPath() const {
  const char *h = getenv("HOME");
  fs::path p = h ? fs::path(h) : fs::current_path();
  p /= ".local/share/OpenPuzzle/openpuzzle.db";
  fs::create_directories(p.parent_path());
  return p.string();
}
bool Application::ensureDb(Database &db) {
  return db.open(dbPath()) && db.createSchema();
}
bool Application::hasArg(const std::vector<std::string> &a,
                         const std::string &n) {
  for (auto &s : a)
    if (s == n)
      return true;
  return false;
}
std::string Application::getArg(const std::vector<std::string> &a,
                                const std::string &n, const std::string &d) {
  for (size_t i = 0; i + 1 < a.size(); ++i)
    if (a[i] == n)
      return a[i + 1];
  return d;
}
int Application::getIntArg(const std::vector<std::string> &a,
                           const std::string &n, int d) {
  auto s = getArg(a, n, "");
  return s.empty() ? d : std::stoi(s);
}
int Application::run(int argc, char **argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  if (args.empty()) {
    std::cout << "OpenPuzzle 0.11.1-dev\n";
    return 0;
  }
  auto cmd = args[0];
  std::vector<std::string> r(args.begin() + 1, args.end());
  try {
    if (cmd == "init")
      return cmdInit();
    if (cmd == "import-puzzle-json")
      return cmdImportPuzzleJson(r);
    if (cmd == "list-puzzles")
      return cmdListPuzzles();
    if (cmd == "sync-data")
      return cmdSyncData(r);
    if (cmd == "puzzle")
      return cmdPuzzle(r);
    if (cmd == "worker")
      return cmdWorker(r);
    if (cmd == "queue")
      return cmdQueue(r);
    if (cmd == "create-job")
      return cmdCreateJob(r);
    if (cmd == "list-ranges")
      return cmdListRanges(r);
    if (cmd == "complete-job")
      return cmdCompleteJob(r);
    if (cmd == "stats")
      return cmdStats(r);
    if (cmd == "configure-tool")
      return cmdConfigureTool(r);
    if (cmd == "tools")
      return cmdTools();
    if (cmd == "gpu-list")
      return cmdGpuList();
    if (cmd == "gpu-select")
      return cmdGpuSelect(r);
    if (cmd == "bitcrack-command")
      return cmdBitcrackCommand(r);
    if (cmd == "start-job")
      return StartJobCommand().run(r);
    if (cmd == "process-test")
      return cmdProcessTest(r);
    if (cmd == "execution-test")
      return cmdExecutionTest(r);
    if (cmd == "session-test")
      return cmdSessionTest(r);
    if (cmd == "event-test")
      return cmdEventTest(r);
    if (cmd == "resume")
      return cmdResume(r);
    if (cmd == "resume-test")
      return cmdResumeTest(r);
    if (cmd == "parse-bitcrack-line")
      return cmdParseBitCrackLine(r);
    if (cmd == "dashboard") {
      Database db;
      if (!ensureDb(db))
        return 1;

      DashboardService service(db);
      return service.execute(r);
    }

    if (cmd == "daemon") {
      Database db;
      if (!ensureDb(db))
        return 1;

      DaemonService service(db);
      return service.execute(r);
    }
    if (cmd == "audit")
      return cmdAudit(r);
    if (cmd == "benchmark")
      return BenchmarkCommand().run(r);
    if (cmd == "dispatch")
      return DispatchCommand().run(r);

    if (cmd == "worker-run")
      return WorkerRunCommand().run(r);


    if (cmd == "doctor") {
      Database db;
      if (!ensureDb(db))
        return 1;

      DoctorService service(db);
      return service.execute(r);
    }
    if (cmd == "engine") {
      Database db;
      if (!ensureDb(db))
        return 1;

      EngineService service(db);
      return service.execute(r);
    }

    if (cmd == "execution") {
      Database db;
      if (!ensureDb(db))
        return 1;

      ExecutionService service(db);
      return service.execute(r);
    }
    if (cmd == "profile")
      return ProfileCommand().run(r);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  std::cerr << "Unknown command\n";
  return 1;
}
int Application::cmdInit() {
  Database db;
  if (!ensureDb(db))
    return 1;
  std::cout << "Database initialized: " << dbPath() << "\n";
  return 0;
}
static std::string readFile(const std::string &f) {
  for (auto p : {fs::path(f), fs::current_path() / f,
                 fs::current_path().parent_path() / f,
                 fs::current_path().parent_path() / "resources/puzzles" / f}) {
    std::ifstream in(p);
    if (in) {
      std::stringstream b;
      b << in.rdbuf();
      return b.str();
    }
  }
  throw std::runtime_error("Could not open puzzle JSON: " + f);
}
static std::string js(const std::string &t, const std::string &k,
                      const std::string &d = "") {
  auto p = t.find("\"" + k + "\"");
  if (p == std::string::npos)
    return d;
  auto c = t.find(':', p);
  auto f = t.find('"', c);
  auto s = t.find('"', f + 1);
  if (f == std::string::npos || s == std::string::npos)
    return d;
  return t.substr(f + 1, s - f - 1);
}
static int ji(const std::string &t, const std::string &k) {
  auto p = t.find("\"" + k + "\"");
  auto c = t.find(':', p);
  return std::stoi(t.substr(c + 1));
}
static double jd(const std::string &t, const std::string &k) {
  auto p = t.find("\"" + k + "\"");
  auto c = t.find(':', p);
  return std::stod(t.substr(c + 1));
}
int Application::cmdImportPuzzleJson(const std::vector<std::string> &a) {
  auto file = getArg(a, "--file");
  auto t = readFile(file);
  PuzzleRecord p;
  p.number = ji(t, "number");
  p.name = js(t, "name");
  p.address = js(t, "address");
  p.reward = jd(t, "reward");
  p.sharing = js(t, "sharing", "private");
  auto ks = js(t, "keyspace");
  auto pos = ks.find(':');
  p.rangeStart = ks.substr(0, pos);
  p.rangeEnd = ks.substr(pos + 1);
  Database db;
  if (!ensureDb(db))
    return 1;
  db.upsertPuzzle(p);
  std::cout << "Imported puzzle " << p.number << "\n";
  return 0;
}
int Application::cmdListPuzzles() {
  Database db;
  if (!ensureDb(db))
    return 1;
  for (auto &p : db.listPuzzles())
    std::cout << "#" << p.number << " " << p.name << " " << p.rangeStart << ":"
              << p.rangeEnd << "\n";
  return 0;
}

static std::vector<std::string> extractJsonStringsArray(const std::string &txt,
                                                        const std::string &key) {
  std::vector<std::string> out;
  auto p = txt.find("\"" + key + "\"");
  if (p == std::string::npos)
    return out;
  auto a = txt.find('[', p);
  auto b = txt.find(']', a);
  if (a == std::string::npos || b == std::string::npos)
    return out;

  std::regex re("\"([^\"]*)\"");
  auto begin = std::sregex_iterator(txt.begin() + a, txt.begin() + b, re);
  auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it)
    out.push_back((*it)[1]);

  return out;
}

struct ImportedRangeItem {
  std::string min;
  std::string max;
  int status = 0;
};

static std::vector<ImportedRangeItem> extractRangesArray(const std::string &txt) {
  std::vector<ImportedRangeItem> out;
  std::regex re(
      R"JSON(\{\s*"min"\s*:\s*"([^"]+)"\s*,\s*"max"\s*:\s*"([^"]+)"\s*,\s*"status"\s*:\s*([0-9]+)\s*\})JSON");

  auto begin = std::sregex_iterator(txt.begin(), txt.end(), re);
  auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it) {
    ImportedRangeItem r;
    r.min = (*it)[1];
    r.max = (*it)[2];
    r.status = std::stoi((*it)[3]);
    out.push_back(r);
  }

  return out;
}

static std::string strip0x(std::string v) {
  if (v.size() > 1 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X'))
    return v.substr(2);
  return v;
}

int Application::cmdSyncData(const std::vector<std::string> &a) {
  auto dir = getArg(a, "--dir", "data");

  auto wallets = extractJsonStringsArray(readFile((fs::path(dir) / "wallets.json").string()), "wallets");
  auto hash160s = extractJsonStringsArray(readFile((fs::path(dir) / "hash160s.json").string()), "hash160s");
  auto ranges = extractRangesArray(readFile((fs::path(dir) / "ranges.json").string()));

  if (wallets.empty())
    throw std::runtime_error("wallets.json has no wallets");
  if (ranges.empty())
    throw std::runtime_error("ranges.json has no ranges");
  if (hash160s.empty())
    throw std::runtime_error("hash160s.json has no hash160s");

  size_t count = std::min<size_t>(160, std::min(wallets.size(), std::min(hash160s.size(), ranges.size())));

  Database db;
  if (!ensureDb(db))
    return 1;

  for (size_t i = 0; i < count; ++i) {
    PuzzleRecord p;
    p.number = static_cast<int>(i + 1);
    p.name = "Puzzle " + std::to_string(i + 1);
    p.address = wallets[i];
    p.hash160 = hash160s[i];
    p.rangeStart = strip0x(ranges[i].min);
    p.rangeEnd = strip0x(ranges[i].max);
    p.reward = static_cast<double>(i + 1);
    p.solved = ranges[i].status != 0;
    p.sharing = "public";
    db.upsertPuzzle(p);
  }

  std::cout << "Wallets imported..... " << wallets.size() << "\n";
  std::cout << "Ranges imported...... " << ranges.size() << "\n";
  std::cout << "Hash160 imported..... " << hash160s.size() << "\n";
  std::cout << "Puzzles synchronized. " << count << "\n";

  if (wallets.size() != ranges.size() || wallets.size() != hash160s.size()) {
    std::cout << "Warning.............. input counts differ; synchronized first "
              << count << " puzzles only\n";
  }

  return 0;
}



int Application::cmdQueue(const std::vector<std::string> &a) {
  Database db;
  if (!ensureDb(db))
    return 1;

  QueueService service(db);
  return service.execute(a);
}

int Application::cmdWorker(const std::vector<std::string> &a) {
  Database db;
  if (!ensureDb(db))
    return 1;

  WorkerService service(db);
  return service.execute(a);
}

int Application::cmdPuzzle(const std::vector<std::string> &a) {
  Database db;
  if (!ensureDb(db))
    return 1;

  PuzzleService service(db);
  return service.execute(a);
}

int Application::cmdCreateJob(const std::vector<std::string> &a) {
  int n = getIntArg(a, "--puzzle", 71), bits = getIntArg(a, "--block-bits", 40);
  Database db;
  if (!ensureDb(db))
    return 1;
  auto p = db.getPuzzleByNumber(n);
  if (!p)
    throw std::runtime_error("Puzzle not found");
  RangeAllocator al(db);
  auto range = al.allocateNext(*p, bits);
  if (!range)
    throw std::runtime_error("No range available");
  JobRecord j;
  j.puzzleId = p->id;
  j.rangeId = range->id;
  j.id = db.insertJob(j);
  std::cout << "Reserved Job ID...... " << j.id << "\nReserved Range ID.... "
            << range->id << "\nKeyspace............. " << range->startKey << ":"
            << range->endKey << "\n";
  return 0;
}
int Application::cmdListRanges(const std::vector<std::string> &a) {
  int n = getIntArg(a, "--puzzle", 71);
  Database db;
  if (!ensureDb(db))
    return 1;
  auto p = db.getPuzzleByNumber(n);
  if (!p)
    throw std::runtime_error("Puzzle not found");
  for (auto &r : db.listRanges(p->id))
    std::cout << "#" << r.id << " " << r.startKey << ":" << r.endKey << " "
              << st(r.status) << " block_bits=" << r.blockBits << "\n";
  return 0;
}
int Application::cmdCompleteJob(const std::vector<std::string> &a) {
  int id = getIntArg(a, "--job", 0);
  Database db;
  if (!ensureDb(db))
    return 1;
  auto j = db.getJob(id);
  if (!j)
    throw std::runtime_error("Job not found");
  db.updateJobState(id, JobState::Completed);
  db.updateRangeStatus(j->rangeId, RangeStatus::Completed);
  std::cout << "Job completed......... " << id << "\nRange completed....... "
            << j->rangeId << "\n";
  return 0;
}
int Application::cmdStats(const std::vector<std::string> &a) {
  int n = getIntArg(a, "--puzzle", 71);
  Database db;
  if (!ensureDb(db))
    return 1;
  auto p = db.getPuzzleByNumber(n);
  if (!p)
    throw std::runtime_error("Puzzle not found");
  std::cout << "Puzzle............... " << p->name << "\nRanges RESERVED...... "
            << db.countRangesByStatus(p->id, RangeStatus::Reserved)
            << "\nRanges RUNNING....... "
            << db.countRangesByStatus(p->id, RangeStatus::Running)
            << "\nRanges COMPLETED..... "
            << db.countRangesByStatus(p->id, RangeStatus::Completed)
            << "\nJobs RESERVED........ "
            << db.countJobsByState(p->id, JobState::Reserved)
            << "\nJobs RUNNING......... "
            << db.countJobsByState(p->id, JobState::Running)
            << "\nJobs COMPLETED....... "
            << db.countJobsByState(p->id, JobState::Completed) << "\n";
  return 0;
}
int Application::cmdConfigureTool(const std::vector<std::string> &a) {
  auto p = getArg(a, "--bitcrack");
  if (p.empty())
    throw std::runtime_error("--bitcrack required");
  ToolManager::configureBitCrack(p);
  std::cout << "Configured BitCrack... " << p << "\n";
  return 0;
}
int Application::cmdTools() {
  auto p = ToolManager::bitcrackPath();
  std::cout << "BitCrack............. " << (p ? *p : "(not configured)")
            << "\n";
  return 0;
}
int Application::cmdGpuList() {
  for (auto &g : GpuManager::listGpus())
    std::cout << "GPU " << g.device << ": " << g.name << " | " << g.memory
              << " | " << g.uuid << "\n";
  return 0;
}
int Application::cmdGpuSelect(const std::vector<std::string> &a) {
  int d = getIntArg(a, "--device", 0);
  GpuManager::selectGpu(d);
  std::cout << "Selected GPU device... " << d << "\n";
  return 0;
}

int Application::cmdBitcrackCommand(const std::vector<std::string> &a) {
  int n = getIntArg(a, "--puzzle", 71), jid = getIntArg(a, "--job", 0),
      b = getIntArg(a, "--blocks", getIntArg(a, "--b", 256)),
      t = getIntArg(a, "--threads", getIntArg(a, "--t", 256)),
      pt = getIntArg(a, "--points", getIntArg(a, "--p", 256)),
      dev = getIntArg(a, "--device",
                      getIntArg(a, "--d", GpuManager::selectedGpu()));
  Database db;
  if (!ensureDb(db))
    return 1;
  auto p = db.getPuzzleByNumber(n);
  auto j = db.getJob(jid);
  if (!p || !j)
    throw std::runtime_error("Puzzle/job not found");
  auto r = db.getRange(j->rangeId);
  auto bc = ToolManager::bitcrackPath();
  if (!r || !bc)
    throw std::runtime_error("Range/BitCrack not found");
  Scheduler scheduler;
  auto workspace = scheduler.workspaceForJob(jid);
  auto out = (fs::path(workspace) / "found.txt").string();

  std::cout << scheduler.buildBitCrackCommand(*bc, *p, *r, dev, b, t, pt, out)
            << "\n";
  return 0;
}

int Application::cmdAudit(const std::vector<std::string> &args) {
  (void)args;
  std::cout << "Audit log is stored in SQLite table: audit_log\n";
  std::cout
      << "Detailed audit listing will be implemented in the next build.\n";
  return 0;
}

} // namespace openpuzzle

namespace openpuzzle {

int Application::cmdParseBitCrackLine(const std::vector<std::string> &args) {
  const std::string line = getArg(args, "--line");

  if (line.empty()) {
    std::cerr << "--line is required\n";
    return 1;
  }

  bitcrack::BitCrackOutputParser parser;
  auto parsed = parser.parse(line);

  std::cout << "Type................. ";

  switch (parsed.type) {
  case bitcrack::ParsedLineType::Unknown:
    std::cout << "UNKNOWN\n";
    break;
  case bitcrack::ParsedLineType::Speed:
    std::cout << "SPEED\n";
    std::cout << "MKey/s............... " << parsed.speedMKeys << "\n";
    break;
  case bitcrack::ParsedLineType::StartingKey:
    std::cout << "STARTING_KEY\n";
    std::cout << "Value................ " << parsed.value << "\n";
    break;
  case bitcrack::ParsedLineType::EndingKey:
    std::cout << "ENDING_KEY\n";
    std::cout << "Value................ " << parsed.value << "\n";
    break;
  case bitcrack::ParsedLineType::CountingBy:
    std::cout << "COUNTING_BY\n";
    std::cout << "Value................ " << parsed.value << "\n";
    break;
  case bitcrack::ParsedLineType::Found:
    std::cout << "FOUND\n";
    std::cout << "Value................ " << parsed.value << "\n";
    break;
  case bitcrack::ParsedLineType::Error:
    std::cout << "ERROR\n";
    std::cout << "Value................ " << parsed.value << "\n";
    break;
  case bitcrack::ParsedLineType::Finished:
    std::cout << "FINISHED\n";
    std::cout << "Value................ " << parsed.value << "\n";
    break;
  }

  return 0;
}

} // namespace openpuzzle

namespace openpuzzle {} // namespace openpuzzle

namespace openpuzzle {} // namespace openpuzzle

namespace openpuzzle {} // namespace openpuzzle

namespace openpuzzle {} // namespace openpuzzle
