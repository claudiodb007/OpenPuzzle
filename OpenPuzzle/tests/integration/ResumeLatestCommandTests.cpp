#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

static std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

static void writeState(const std::filesystem::path &jobPath,
                       const std::string &status, double speed) {
  std::filesystem::create_directories(jobPath);

  std::ofstream state(jobPath / "state.json");
  state << "{\n";
  state << "  \"status\": \"" << status << "\",\n";
  state << "  \"exit_code\": 0,\n";
  state << "  \"lines_read\": 2,\n";
  state << "  \"average_speed\": " << speed << "\n";
  state << "}\n";
}

int main() {
  auto temp =
      std::filesystem::temp_directory_path() / "openpuzzle_resume_latest_test";
  auto output = std::filesystem::temp_directory_path() /
                "openpuzzle_resume_latest_output.txt";

  std::filesystem::remove_all(temp);
  std::filesystem::remove(output);

  writeState(temp / "jobs" / "00000041", "FINISHED", 1000.0);
  writeState(temp / "jobs" / "00000042", "FINISHED", 1334.62);

  std::string command =
      "./OpenPuzzle resume --base " + temp.string() + " > " + output.string();

  int code = std::system(command.c_str());

  if (code != 0)
    return 1;

  auto content = readFile(output);

  if (content.find("Job............. 42") == std::string::npos)
    return 2;
  if (content.find("Speed........... 1334.62 MKey/s") == std::string::npos)
    return 3;

  std::filesystem::remove_all(temp);
  std::filesystem::remove(output);

  return 0;
}
