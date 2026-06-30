#pragma once

#include <filesystem>
#include <string>

namespace openpuzzle {

class WorkspaceManager {
public:
    explicit WorkspaceManager(std::filesystem::path basePath);

    std::filesystem::path basePath() const;
    std::filesystem::path jobsPath() const;
    std::filesystem::path jobWorkspace(int jobId) const;

    std::filesystem::path createJobWorkspace(int jobId) const;

    std::filesystem::path bitcrackLog(int jobId) const;
    std::filesystem::path stdoutLog(int jobId) const;
    std::filesystem::path stderrLog(int jobId) const;
    std::filesystem::path executionFile(int jobId) const;
    std::filesystem::path stateFile(int jobId) const;

private:
    std::filesystem::path basePath_;
    static std::string formatJobId(int jobId);
};

} // namespace openpuzzle
