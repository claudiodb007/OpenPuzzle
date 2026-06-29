#pragma once
#include "openpuzzle/models/Models.hpp"
#include <sqlite3.h>
#include <optional>
#include <string>
#include <vector>
namespace openpuzzle {
class Database {
public:
    ~Database(); bool open(const std::string& path); void close(); bool createSchema();
    bool upsertPuzzle(const PuzzleRecord& puzzle); std::optional<PuzzleRecord> getPuzzleByNumber(int number); std::vector<PuzzleRecord> listPuzzles();
    int insertRange(const RangeRecord& range); std::vector<RangeRecord> listRanges(int puzzleId); std::optional<RangeRecord> getRange(int rangeId); bool updateRangeStatus(int rangeId, RangeStatus status);
    int insertJob(const JobRecord& job); std::optional<JobRecord> getJob(int jobId); bool updateJobState(int jobId, JobState state);
    int insertExecution(int jobId, const std::string& workspace, const std::string& command, const std::string& state); bool finishExecution(int executionId, const std::string& state, int exitCode); int insertStatistic(int executionId, double speedMkeys, double temperature, double power);
    int insertProgress(int executionId, const std::string& currentKey, const std::string& keysProcessed,
                       double speedMkeys, double progressPercent, const std::string& eta);
    bool insertAuditLog(int puzzleId, int rangeId, int jobId, int executionId,
                        const std::string& event, const std::string& message);
    bool updateRangeKeysChecked(int rangeId, const std::string& keysChecked);
    bool insertExternalRange(int puzzleId, const std::string& startKey, const std::string& endKey, const std::string& source, const std::string& confidence, const std::string& notes); std::vector<RangeRecord> listExternalRanges(int puzzleId);
    long long countRangesByStatus(int puzzleId, RangeStatus status); long long countJobsByState(int puzzleId, JobState state);
private: sqlite3* db_=nullptr; bool exec(const std::string& sql);
};
}
