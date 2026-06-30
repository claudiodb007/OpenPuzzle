#include <cstdlib>
#include <filesystem>

int main()
{
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_resume_missing_state_test";
    std::filesystem::remove_all(temp);

    std::filesystem::create_directories(temp / "jobs" / "00000042");

    std::string command =
        "./OpenPuzzle resume --job 42 --base " + temp.string() + " > /tmp/openpuzzle_resume_missing_stdout.txt 2> /tmp/openpuzzle_resume_missing_stderr.txt";

    int code = std::system(command.c_str());

    std::filesystem::remove_all(temp);

    return code != 0 ? 0 : 1;
}
