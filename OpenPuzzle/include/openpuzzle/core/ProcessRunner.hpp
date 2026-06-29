#pragma once

#include <functional>
#include <string>

namespace openpuzzle {

struct ProcessResult {
    int exitCode = -1;
    bool started = false;
};

class ProcessRunner {
public:
    using LineCallback = std::function<void(const std::string&)>;

    ProcessResult run(const std::string& command, const LineCallback& onLine) const;
};

}
