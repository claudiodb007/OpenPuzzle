#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"

#include <sstream>

namespace openpuzzle {

BitCrackEngine::BitCrackEngine(std::string executable)
    : executable_(std::move(executable)) {}

EngineInfo BitCrackEngine::info() const {
    EngineInfo info;
    info.name = "BitCrack";
    info.version = "unknown";
    info.backend = "CUDA/OpenCL";
    info.executable = executable_;
    info.available = !executable_.empty();
    return info;
}

bool BitCrackEngine::prepare() {
    return true;
}


std::string BitCrackEngine::buildCommand(const EngineLaunchRequest& request) const {
    std::ostringstream command;

    command << executable_ << " "
            << request.puzzle.address
            << " --keyspace "
            << request.range.startKey
            << ":"
            << request.range.endKey
            << " --out "
            << request.outputFile
            << " -d "
            << request.device
            << " -b "
            << request.blocks
            << " -t "
            << request.threads
            << " -p "
            << request.points
            << " 2>&1 | tee -a "
            << request.logFile;

    return command.str();
}

bool BitCrackEngine::launch() {
    running_ = true;
    return true;
}

bool BitCrackEngine::stop() {
    running_ = false;
    return true;
}

bool BitCrackEngine::running() const {
    return running_;
}

} // namespace openpuzzle
