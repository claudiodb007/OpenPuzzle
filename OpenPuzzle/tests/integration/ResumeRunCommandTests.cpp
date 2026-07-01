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

int main() {
  auto temp =
      std::filesystem::temp_directory_path() / "openpuzzle_resume_run_test";
  auto output = std::filesystem::temp_directory_path() /
                "openpuzzle_resume_run_output.txt";

  std::filesystem::remove_all(temp);
  std::filesystem::remove(output);

  auto jobPath = temp / "jobs" / "00000042";
  std::filesystem::create_directories(jobPath);

  {
    std::ofstream state(jobPath / "state.json");
    state << "{\n";
    state << "  \"status\": \"FINISHED\",\n";
    state << "  \"exit_code\": 0,\n";
    state << "  \"lines_read\": 2,\n";
    state << "  \"average_speed\": 1334.62\n";
    state << "}\n";
  }

  {
    std::ofstream execution(jobPath / "execution.json");
    execution << "{\n";
    execution << "  \"execution_id\": 1,\n";
    execution << "  \"puzzle_id\": 71,\n";
    execution << "  \"job_id\": 42,\n";
    execution << "  \"range_id\": 1001,\n";
    execution << "  \"engine\": \"BitCrack\",\n";
    execution
        << "  \"command\": \"printf '[Info] 1500.00 MKey/s\\nFinished\\n'\",\n";
    execution << "  \"workspace\": \"" << jobPath.string() << "\",\n";
    execution << "  \"echo_output\": true\n";
    execution << "}\n";
  }

  std::string command = "./OpenPuzzle resume --job 42 --base " + temp.string() +
                        " --run > " + output.string();

  int code = std::system(command.c_str());

  if (code != 0)
    return 1;

  auto content = readFile(output);

  if (content.find("OpenPuzzle Recovery") == std::string::npos)
    return 2;
  if (content.find("Rebuilding execution context") == std::string::npos)
    return 3;
  if (content.find("[Info] 1500.00 MKey/s") == std::string::npos)
    return 4;
  if (content.find("Execution finished") == std::string::npos)
    return 5;
  if (content.find("Average speed... 1500 MKey/s") == std::string::npos)
    return 6;

  std::filesystem::remove_all(temp);
  std::filesystem::remove(output);

  return 0;
}
