#pragma once
#include <string>
namespace openpuzzle {
enum class RangeStatus {
  Reserved = 1,
  Running = 2,
  Completed = 3,
  Failed = 4,
  Cancelled = 5,
  External = 6
};
enum class JobState {
  Reserved = 1,
  Running = 2,
  Completed = 3,
  Failed = 4,
  Cancelled = 5
};
struct PuzzleRecord {
  int id = 0;
  int number = 0;
  std::string name;
  std::string address;
  std::string hash160;
  std::string rangeStart;
  std::string rangeEnd;
  double reward = 0.0;
  bool solved = false;
  std::string solvedKey;
  std::string solvedAddress;
  std::string sharing;
};
struct RangeRecord {
  int id = 0;
  int puzzleId = 0;
  std::string startKey;
  std::string endKey;
  int blockBits = 0;
  RangeStatus status = RangeStatus::Reserved;
  std::string keysChecked = "0";
};
struct JobRecord {
  int id = 0;
  int puzzleId = 0;
  int rangeId = 0;
  JobState state = JobState::Reserved;
};
struct GpuProfileRecord {
  int id = 0;
  std::string gpuName;
  std::string backend;
  std::string engine;

  int blocks = 0;
  int threads = 0;
  int points = 0;

  double averageSpeed = 0.0;
  double minimumSpeed = 0.0;
  double maximumSpeed = 0.0;

  int samples = 0;
};

} // namespace openpuzzle
