#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "../../OpenPuzzle-Vanity/source/client/openpuzzle/runtime/VanityRangeProtocol.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace {
namespace fs = std::filesystem;
using namespace openpuzzle;

fs::path executableDirectory() {
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length == path.size()) {
        throw std::runtime_error("Could not locate OpenPuzzle.");
    }
    return fs::path(path.data()).parent_path();
}

int deviceIndex(std::string_view text) {
    int result = -1;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc() || end != text.data() + text.size() || result < 0 || result > 255) {
        throw std::runtime_error("Use a CUDA device index between 0 and 255.");
    }
    return result;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "OpenPuzzle Windows Vanity CUDA preview 1\n";
        return 0;
    }
    try {
        if (argc > 2) {
            throw std::runtime_error("Usage: openpuzzle-vanity-cuda-test.exe [cuda-device-index]");
        }
        const auto device = argc == 2 ? deviceIndex(argv[1]) : 0;
        const auto engine = executableDirectory() / L"engines" / L"vanity-offline.exe";
        const auto workspace = localPaths::root() / L"vanity-cuda-test" /
            (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        const auto targets = workspace / L"targets.txt";
        const auto results = workspace / L"solutions.txt";

        // Use the final key of a small synthetic interval to check both coverage
        // and a CPU-verified hit. No request is sent to the coordinator.
        const vanityPreview::Plan plan("10000000000000000", 513, 1);
        constexpr std::string_view expectedHash = "60631ba6afba4d9fd4bb634cc90a7dcd1a2bab74";
        constexpr std::string_view expectedAddress = "19nebZYe1CgNkkCTRVs2MWie2D1aqqZz8t";
        WorkspaceSecurity::prepare(workspace);
        if (!writePrivateFileAtomically(targets, "OPV-TARGETS-1\nhash160 " +
                 std::string(expectedHash) + "\n")) {
            throw std::runtime_error("Could not create the local test target file.");
        }
        if (!SetEnvironmentVariableW(L"CUDA_VISIBLE_DEVICES", std::to_wstring(device).c_str())) {
            throw std::runtime_error("Could not select the CUDA device.");
        }
        auto process = WindowsEngineProcess::start(engine,
            {"--target-file", plan.start, "513", "1", pathUtf8(targets), pathUtf8(results), "exhaust",
             std::to_string(GetCurrentProcessId())}, workspace);
        const auto log = process->logPath();
        std::cout << "CUDA device: " << device << '\n' << "Engine log: " << pathUtf8(log) << '\n' << std::flush;
        const auto exitCode = process->wait(120000);
        if (!exitCode) {
            process->stop();
            throw std::runtime_error("The CUDA test timed out.");
        }
        bool missing = false;
        const auto output = windowsFile::readPrivate(log, missing, 65536);
        if (!output) {
            throw std::runtime_error("Could not read the local engine log.");
        }
        std::cout << *output;
        if (*exitCode != 0) {
            std::ostringstream message;
            message << "Vanity engine exited with code " << *exitCode
                    << " (0x" << std::hex << std::uppercase << std::setw(8)
                    << std::setfill('0') << *exitCode << ").";
            if (output->find("OPV1 BEGIN ") == std::string::npos) {
                message << " The engine stopped before starting the test range.";
            }
            throw std::runtime_error(message.str());
        }
        vanityPreview::Protocol protocol(plan);
        std::istringstream events(*output);
        for (std::string line; std::getline(events, line);) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            protocol.consume(line);
        }

        const auto match = windowsFile::readPrivate(results, missing, 4096);
        const auto expected = "MATCH " + plan.end + ' ' + std::string(expectedHash) + ' ' +
            std::string(expectedAddress) + ' ';
        if (!protocol.completed(0) || !protocol.traced ||
            !match || match->rfind(expected, 0) != 0 || std::count(match->begin(), match->end(), '\n') != 1) {
            throw std::runtime_error("The CUDA test did not verify the expected range and match.");
        }
        std::cout << "PASS: Vanity CUDA verified 513 keys and found the synthetic target.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
