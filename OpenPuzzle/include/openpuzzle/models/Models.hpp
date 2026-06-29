#pragma once
#include <string>
namespace openpuzzle {
enum class RangeStatus { Reserved=1, Running=2, Completed=3, Failed=4, Cancelled=5, External=6 };
enum class JobState { Reserved=1, Running=2, Completed=3, Failed=4, Cancelled=5 };
struct PuzzleRecord { int id=0; int number=0; std::string name; std::string address; std::string rangeStart; std::string rangeEnd; double reward=0.0; std::string sharing; };
struct RangeRecord { int id=0; int puzzleId=0; std::string startKey; std::string endKey; int blockBits=0; RangeStatus status=RangeStatus::Reserved; };
struct JobRecord { int id=0; int puzzleId=0; int rangeId=0; JobState state=JobState::Reserved; };
}
