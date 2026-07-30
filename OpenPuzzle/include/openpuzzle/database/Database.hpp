#pragma once
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/core/ExecutionRecord.hpp"
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>
namespace openpuzzle {

struct AuditLogRecord {
  int id = 0;
  int puzzleId = 0;
  int rangeId = 0;
  int jobId = 0;
  int executionId = 0;
  std::string event;
  std::string message;
  std::string createdAt;
};

class Database {
public:
  ~Database();
  bool open(const std::string &path);
  void close();
  bool createSchema();
  bool upsertPuzzle(const PuzzleRecord &puzzle);
  bool updatePuzzleHash160(int number, const std::string &hash160);
  std::optional<PuzzleRecord> getPuzzleByNumber(int number);
  std::optional<PuzzleRecord> getPuzzleById(int id);
  std::vector<PuzzleRecord> listPuzzles();
  int insertRange(const RangeRecord &range);
  std::vector<RangeRecord> listRanges(int puzzleId);
  std::optional<RangeRecord> getRange(int rangeId);
  bool updateRangeStatus(int rangeId, RangeStatus status);
  int insertJob(const JobRecord &job);
  std::optional<JobRecord> getJob(int jobId);
  std::optional<JobRecord> nextReservedJob();
  bool updateJobState(int jobId, JobState state);
  int insertExecution(int jobId, const std::string &workspace,
                      const std::string &command, const std::string &state);
  bool finishExecution(int executionId, const std::string &state, int exitCode);
  std::optional<ExecutionRecord> getExecution(int executionId);
  std::vector<ExecutionRecord> listExecutions();
  std::vector<ExecutionRecord> listRunningExecutions();
  int insertStatistic(int executionId, double speedMkeys, double temperature,
                      double power);
  int insertProgress(int executionId, const std::string &currentKey,
                     const std::string &keysProcessed, double speedMkeys,
                     double progressPercent, const std::string &eta);
  bool insertAuditLog(int puzzleId, int rangeId, int jobId, int executionId,
                      const std::string &event, const std::string &message);
  std::vector<AuditLogRecord> listAuditLog(
      int limit,
      std::optional<int> puzzleId = std::nullopt,
      const std::string &event = {});
  bool updateRangeKeysChecked(int rangeId, const std::string &keysChecked);
  bool insertExternalRange(int puzzleId, const std::string &startKey,
                           const std::string &endKey, const std::string &source,
                           const std::string &confidence,
                           const std::string &notes);
  std::vector<RangeRecord> listExternalRanges(int puzzleId);

  bool saveGpuProfile(const GpuProfileRecord &profile);
  std::optional<GpuProfileRecord> getGpuProfile(const std::string &gpuName,
                                                const std::string &backend,
                                                const std::string &engine);
  std::vector<GpuProfileRecord> listGpuProfiles();
  long long countRangesByStatus(int puzzleId, RangeStatus status);
  long long countJobsByState(int puzzleId, JobState state);

  int upsertWorker(const WorkerRecord &worker);
  bool updateWorkerStatus(int workerId, const std::string& status);

  bool updateWorkerHeartbeat(
      int workerId,
      const std::string& status,
      double speedMkeys,
      double temperature,
      double power);

  int markStaleWorkersOffline(int timeoutSeconds);

  std::vector<WorkerRecord> listWorkers();
  std::optional<WorkerRecord> getWorker(int workerId);

private:
  sqlite3 *db_ = nullptr;
  bool exec(const std::string &sql);
};
} // namespace openpuzzle
