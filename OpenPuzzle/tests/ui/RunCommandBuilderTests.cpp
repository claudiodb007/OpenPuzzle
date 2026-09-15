#include "openpuzzle/ui/RunCommandBuilder.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

using openpuzzle::ui::RunCommandBuilder;
using openpuzzle::ui::RunMode;
using openpuzzle::ui::RunSelection;

namespace {

bool rejected(const RunSelection& selection) {
  try {
    (void) RunCommandBuilder::build(selection);
  } catch (const std::invalid_argument&) {
    return true;
  }

  return false;
}

} // namespace

int main() {
  assert(
      RunCommandBuilder::build({
          71, RunMode::BitCrackCuda, 0, 0, 1, false,
      }) ==
      std::vector<std::string>({
          "run", "71",
          "--engine", "bitcrack",
          "--backend", "cuda",
          "--device", "0",
      }));

  assert(
      RunCommandBuilder::build({
          71, RunMode::BitCrackOpencl, 1, 0, 1, true,
      }) ==
      std::vector<std::string>({
          "run", "71",
          "--engine", "bitcrack",
          "--backend", "opencl",
          "--device", "1",
          "--rusticl-enable", "radeonsi",
      }));

  assert(
      RunCommandBuilder::build({
          71, RunMode::BitCrackCudaOpencl, 0, 1, 1, true,
      }) ==
      std::vector<std::string>({
          "run", "71",
          "--engine", "bitcrack",
          "--backend", "cuda",
          "--device", "0",
          "--with-opencl",
          "--opencl-device", "1",
          "--rusticl-enable", "radeonsi",
      }));

  assert(
      RunCommandBuilder::build({
          71, RunMode::KeyHuntCpu, 0, 0, 8, false,
      }) ==
      std::vector<std::string>({
          "run", "71",
          "--engine", "keyhunt",
          "--backend", "cpu",
          "--threads", "8",
      }));

  for (const int puzzle : {140, 145, 150, 155, 160}) {
    assert(RunCommandBuilder::supportsPuzzle(
        RunMode::KangarooCuda,
        puzzle));
    assert(
        RunCommandBuilder::build({
            puzzle, RunMode::KangarooCuda, 0, 0, 1, false,
        }) ==
        std::vector<std::string>({
            "run", std::to_string(puzzle),
            "--engine", "kangaroo",
            "--backend", "cuda",
            "--device", "0",
        }));
  }

  assert(!RunCommandBuilder::supportsPuzzle(
      RunMode::KangarooCuda,
      139));
  assert(!RunCommandBuilder::supportsPuzzle(
      RunMode::KangarooCuda,
      141));

  assert(rejected({
      139, RunMode::KangarooCuda, 0, 0, 1, false}));
  assert(rejected({
      71, RunMode::BitCrackCuda, -1, 0, 1, false}));
  assert(rejected({
      71, RunMode::BitCrackCudaOpencl, 0, -1, 1, false}));
  assert(rejected({
      71, RunMode::KeyHuntCpu, 0, 0, 0, false}));
  assert(rejected({
      0, RunMode::BitCrackCuda, 0, 0, 1, false}));
  assert(rejected({
      71, static_cast<RunMode>(99), 0, 0, 1, false}));

  return 0;
}
