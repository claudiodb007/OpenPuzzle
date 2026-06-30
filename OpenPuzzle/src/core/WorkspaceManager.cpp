#include "openpuzzle/core/WorkspaceManager.hpp"

#include <iomanip>
#include <sstream>

namespace openpuzzle {

WorkspaceManager::WorkspaceManager(std::filesystem::path basePath)
    : basePath_(std::move(basePath)) {}

std::filesystem::path WorkspaceManager::basePath() const {
    return basePath_;
}

std::filesystem::path WorkspaceManager::jobsPath() const {
    return basePath_ / "jobs";
}

std::filesystem::path WorkspaceManager::jobWorkspace(int jobId) const {
    return jobsPath() / formatJobId(jobId);
}

std::filesystem::path WorkspaceManager::createJobWorkspace(int jobId) const {
    auto path = jobWorkspace(jobId);
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path WorkspaceManager::bitcrackLog(int jobId) const {
    return jobWorkspace(jobId) / "bitcrack.log";
}

std::filesystem::path WorkspaceManager::stdoutLog(int jobId) const {
    return jobWorkspace(jobId) / "stdout.log";
}

std::filesystem::path WorkspaceManager::stderrLog(int jobId) const {
    return jobWorkspace(jobId) / "stderr.log";
}

std::filesystem::path WorkspaceManager::executionFile(int jobId) const {
    return jobWorkspace(jobId) / "execution.json";
}

std::filesystem::path WorkspaceManager::stateFile(int jobId) const {
    return jobWorkspace(jobId) / "state.json";
}

std::string WorkspaceManager::formatJobId(int jobId) {
    std::ostringstream out;
    out << std::setw(8) << std::setfill('0') << jobId;
    return out.str();
}

} // namespace openpuzzle
