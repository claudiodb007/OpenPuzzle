#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/database/Database.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace openpuzzle;

namespace {

struct CapturedRun {
  int exitCode = 1;
  std::string output;
};

CapturedRun runApplication(
    std::vector<std::string> arguments) {
  std::vector<char *> argv;

  for (auto &argument : arguments) {
    argv.push_back(argument.data());
  }

  std::ostringstream output;
  auto *oldOutput = std::cout.rdbuf(output.rdbuf());
  auto *oldError = std::cerr.rdbuf(output.rdbuf());

  Application application;
  const int exitCode = application.run(
      static_cast<int>(argv.size()), argv.data());

  std::cout.rdbuf(oldOutput);
  std::cerr.rdbuf(oldError);

  return {exitCode, output.str()};
}

PuzzleRecord puzzle71() {
  PuzzleRecord puzzle;

  puzzle.number = 71;
  puzzle.name = "Puzzle 71";
  puzzle.address = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
  puzzle.rangeStart = "400000000000000000";
  puzzle.rangeEnd = "7FFFFFFFFFFFFFFFFF";
  puzzle.reward = 7.1;
  puzzle.sharing = "public";

  return puzzle;
}

} // namespace

int main() {
  {
    Database database;

    assert(database.open(":memory:"));
    assert(database.createSchema());
    assert(database.upsertPuzzle(puzzle71()));

    const auto puzzle = database.getPuzzleByNumber(71);
    assert(puzzle);

    assert(database.insertAuditLog(
        puzzle->id,
        0,
        0,
        0,
        "assignment_requested",
        "Requested a test assignment"));

    assert(database.insertAuditLog(
        0,
        0,
        0,
        0,
        "runtime_started",
        "Test runtime"));

    const auto all = database.listAuditLog(50);
    assert(all.size() == 2);
    assert(all[0].event == "runtime_started");
    assert(all[1].event == "assignment_requested");

    const auto limited = database.listAuditLog(1);
    assert(limited.size() == 1);
    assert(limited[0].event == "runtime_started");

    const auto puzzleEntries =
        database.listAuditLog(50, puzzle->id);
    assert(puzzleEntries.size() == 1);
    assert(puzzleEntries[0].puzzleId == puzzle->id);

    const auto eventEntries =
        database.listAuditLog(
            50,
            std::nullopt,
            "assignment_requested");
    assert(eventEntries.size() == 1);
    assert(eventEntries[0].message ==
           "Requested a test assignment");
  }

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      "openpuzzle_audit_log_tests";

  std::filesystem::remove_all(temporaryHome);
  std::filesystem::create_directories(temporaryHome);

  const char *existingHome = std::getenv("HOME");
  const std::string savedHome =
      existingHome ? existingHome : "";
  const bool hadHome = existingHome != nullptr;

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  const auto databasePath =
      temporaryHome /
      ".local/share/OpenPuzzle/openpuzzle.db";

  {
    std::filesystem::create_directories(
        databasePath.parent_path());

    Database database;

    assert(database.open(databasePath.string()));
    assert(database.createSchema());
    assert(database.upsertPuzzle(puzzle71()));

    const auto puzzle = database.getPuzzleByNumber(71);
    assert(puzzle);

    assert(database.insertAuditLog(
        puzzle->id,
        42,
        0,
        0,
        "assignment_completed",
        "Range completed successfully") == false);

    assert(database.insertAuditLog(
        puzzle->id,
        0,
        0,
        0,
        "assignment_completed",
        "Range completed successfully"));
  }

  {
    const auto result =
        runApplication({
            "openpuzzle",
            "audit",
            "--limit",
            "5",
            "--puzzle",
            "71",
            "--event",
            "assignment_completed"
        });

    assert(result.exitCode == 0);
    assert(
        result.output.find("OpenPuzzle Audit") !=
        std::string::npos);
    assert(
        result.output.find("Entries............. 1") !=
        std::string::npos);
    assert(
        result.output.find("Puzzle.............. #71") !=
        std::string::npos);
    assert(
        result.output.find("assignment_completed") !=
        std::string::npos);
    assert(
        result.output.find("Range completed successfully") !=
        std::string::npos);
  }

  {
    const auto result =
        runApplication({
            "openpuzzle",
            "audit",
            "--help"
        });

    assert(result.exitCode == 0);
    assert(
        result.output.find("--limit N") !=
        std::string::npos);
    assert(
        result.output.find("--puzzle N") !=
        std::string::npos);
    assert(
        result.output.find("--event NAME") !=
        std::string::npos);
  }

  if (hadHome) {
    assert(setenv("HOME", savedHome.c_str(), 1) == 0);
  } else {
    assert(unsetenv("HOME") == 0);
  }

  std::filesystem::remove_all(temporaryHome);

  std::cout << "AuditLogTests passed\n";

  return 0;
}
