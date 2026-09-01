#include "openpuzzle/client/SolutionExporter.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace openpuzzle::client;
namespace fs = std::filesystem;

namespace {

void writeFile(
    const fs::path &path,
    const std::string &content) {
  std::ofstream output(path);
  output << content;
}

std::string readFile(
    const fs::path &path) {
  std::ifstream input(path);
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

bool ownerOnly(
    const fs::path &path,
    bool directory) {
  const auto permissions =
      fs::status(path).permissions();

  const auto expected =
      directory
          ? fs::perms::owner_all
          : fs::perms::owner_read |
                fs::perms::owner_write;

  return
      (permissions & fs::perms::all) ==
      expected;
}

} // namespace

int main() {
  const fs::path temporaryHome =
      fs::temp_directory_path() /
      ("openpuzzle-solution-exporter-" +
       std::to_string(getpid()));

  fs::remove_all(temporaryHome);
  fs::create_directories(temporaryHome);

  const char *oldHome =
      std::getenv("HOME");

  const bool hadHome =
      oldHome != nullptr;

  const std::string savedHome =
      hadHome
          ? oldHome
          : "";

  assert(
      setenv(
          "HOME",
          temporaryHome.c_str(),
          1) == 0);

  ClientExecutionState state;
  state.active = true;
  state.assignmentId =
      "20202020-2020-4020-8020-202020202020";
  state.clientId =
      "30303030-3030-4030-8030-303030303030";
  state.puzzle = 20;
  state.rangeId = 1;
  state.pid = 999999;
  state.target =
      "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";
  state.start = "1";
  state.end = "FFFFFFFF";
  state.engine = "BitCrack";
  state.backend = "CUDA";
  state.workspace =
      (temporaryHome / "workspace").string();
  state.command = "synthetic-test";

  fs::create_directories(state.workspace);

  const std::string syntheticWif =
      "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn";

  const fs::path engineResult =
      fs::path(state.workspace) /
      "found.txt";

  writeFile(
      engineResult,
      "OPENPUZZLE_SOLUTION_V1\n"
      "address=" + state.target + "\n"
      "private_key_wif=" + syntheticWif + "\n"
      "compression=compressed\n");

  const auto exported =
      SolutionExporter::exportSolution(
          state,
          engineResult.string());

  assert(exported.success);
  assert(exported.format == "WIF (compressed)");

  const fs::path expectedRoot =
      temporaryHome /
      "OpenPuzzle-Solutions";

  const fs::path expectedPuzzle =
      expectedRoot /
      "Puzzle-20";

  const fs::path expectedAssignment =
      expectedPuzzle /
      state.assignmentId;

  const fs::path expectedWallet =
      expectedAssignment /
      "wallet-import.txt";

  const fs::path expectedNotice =
      expectedRoot /
      ("KEY-FOUND-Puzzle-20-" +
       state.assignmentId +
       ".txt");

  assert(
      exported.walletPath ==
      expectedWallet.string());

  assert(
      exported.noticePath ==
      expectedNotice.string());

  assert(exported.warning.empty());

  assert(fs::is_regular_file(expectedWallet));
  assert(fs::is_regular_file(expectedNotice));
  assert(fs::is_regular_file(engineResult));
  assert(ownerOnly(expectedRoot, true));
  assert(ownerOnly(expectedPuzzle, true));
  assert(ownerOnly(expectedAssignment, true));
  assert(ownerOnly(expectedWallet, false));
  assert(ownerOnly(expectedNotice, false));

  const auto walletContent =
      readFile(expectedWallet);

  const auto noticeContent =
      readFile(expectedNotice);

  assert(
      walletContent.find(state.target) !=
      std::string::npos);

  assert(
      walletContent.find(syntheticWif) !=
      std::string::npos);

  assert(
      noticeContent.find(expectedWallet.string()) !=
      std::string::npos);

  assert(
      noticeContent.find(syntheticWif) ==
      std::string::npos);

  const auto repeated =
      SolutionExporter::exportSolution(
          state,
          engineResult.string());

  assert(repeated.success);
  assert(
      repeated.walletPath ==
      expectedWallet.string());
  assert(
      repeated.noticePath ==
      expectedNotice.string());
  assert(repeated.warning.empty());

  /*
   * PSCKangaroo writes a single native hexadecimal result.
   * Accept it only for a Kangaroo assignment and only when
   * the value belongs to the assigned inclusive range.
   */
  {
    auto kangarooState = state;
    kangarooState.assignmentId =
        "40404040-4040-4040-8040-404040404040";
    kangarooState.engine = "PSCKangaroo";
    kangarooState.backend = "Kangaroo";
    kangarooState.start = "180000000";
    kangarooState.end = "1800000FF";
    kangarooState.workspace =
        (temporaryHome / "kangaroo-workspace").string();

    fs::create_directories(kangarooState.workspace);

    const std::string nativePrivateKey =
        "0000000000000000000000000000000000000000000000000000000180000001";

    const fs::path nativeResult =
        fs::path(kangarooState.workspace) /
        "RESULTS.TXT";

    writeFile(
        nativeResult,
        "PRIVATE KEY: " + nativePrivateKey + "\n");

    const auto nativeExport =
        SolutionExporter::exportSolution(
            kangarooState,
            nativeResult.string());

    assert(nativeExport.success);
    assert(nativeExport.format == "hexadecimal");
    assert(
        readFile(nativeExport.walletPath).find(
            nativePrivateKey) != std::string::npos);
    assert(
        readFile(nativeExport.noticePath).find(
            nativePrivateKey) == std::string::npos);

    writeFile(
        nativeResult,
        "PRIVATE KEY: 17FFFFFFF\n");

    const auto outsideRange =
        SolutionExporter::exportSolution(
            kangarooState,
            nativeResult.string());

    assert(!outsideRange.success);

    writeFile(
        nativeResult,
        "PRIVATE KEY: not-hexadecimal\n");

    const auto malformedNative =
        SolutionExporter::exportSolution(
            kangarooState,
            nativeResult.string());

    assert(!malformedNative.success);

    auto bitCrackState = kangarooState;
    bitCrackState.engine = "BitCrack";

    writeFile(
        nativeResult,
        "PRIVATE KEY: " + nativePrivateKey + "\n");

    const auto wrongEngine =
        SolutionExporter::exportSolution(
            bitCrackState,
            nativeResult.string());

    assert(!wrongEngine.success);
  }

  const fs::path wrongAddress =
      fs::path(state.workspace) /
      "wrong-address.txt";

  writeFile(
      wrongAddress,
      "OPENPUZZLE_SOLUTION_V1\n"
      "address=1WrongSyntheticAddress\n"
      "private_key_wif=" + syntheticWif + "\n"
      "compression=compressed\n");

  const auto rejectedAddress =
      SolutionExporter::exportSolution(
          state,
          wrongAddress.string());

  assert(!rejectedAddress.success);

  const fs::path legacyResult =
      fs::path(state.workspace) /
      "legacy.txt";

  writeFile(
      legacyResult,
      "Private Key: synthetic-legacy-value\n");

  const auto rejectedLegacy =
      SolutionExporter::exportSolution(
          state,
          legacyResult.string());

  assert(!rejectedLegacy.success);

  fs::remove_all(temporaryHome);

  if (hadHome) {
    assert(
        setenv(
            "HOME",
            savedHome.c_str(),
            1) == 0);
  } else {
    assert(unsetenv("HOME") == 0);
  }

  std::cout
      << "SolutionExporterTests passed\n";

  return 0;
}
