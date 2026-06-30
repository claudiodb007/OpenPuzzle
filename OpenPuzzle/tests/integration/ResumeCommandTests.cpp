#include <cstdlib>
#include <filesystem>
#include <fstream>

int main()
{
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_resume_command_test";
    std::filesystem::remove_all(temp);

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
        "./OpenPuzzle resume --job 42 --base " + temp.string() + " > /tmp/openpuzzle_resume_command_output.txt";

    int code = std::system(command.c_str());

    std::filesystem::remove_all(temp);

    return code == 0 ? 0 : 1;
}
