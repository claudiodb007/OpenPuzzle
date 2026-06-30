#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

static std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

int main()
{
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_resume_command_test";
    auto output = std::filesystem::temp_directory_path() / "openpuzzle_resume_command_output.txt";

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

    std::string command =
        "./OpenPuzzle resume --job 42 --base " + temp.string() + " > " + output.string();

    int code = std::system(command.c_str());

    if (code != 0) return 1;

    auto content = readFile(output);

    if (content.find("OpenPuzzle Recovery") == std::string::npos) return 2;
    if (content.find("Job............. 42") == std::string::npos) return 3;
    if (content.find("Status.......... FINISHED") == std::string::npos) return 4;
    if (content.find("Speed........... 1334.62 MKey/s") == std::string::npos) return 5;

    std::filesystem::remove_all(temp);
    std::filesystem::remove(output);

    return 0;
}
