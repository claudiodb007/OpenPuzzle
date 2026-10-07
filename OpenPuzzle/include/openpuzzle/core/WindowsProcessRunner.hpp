#pragma once

#include "openpuzzle/core/IProcessRunner.hpp"

namespace openpuzzle {
// Commands use Windows cmd.exe syntax. Commands are UTF-8; output bytes pass through unchanged.
// This runner owns the command's complete job and terminates it on cancellation.
class WindowsProcessRunner final : public IProcessRunner {
public:
  ProcessResult run(const std::string &command, const LineCallback &onLine,
                    int maxSeconds = 0,
                    const StopPredicate &stop = nullptr) const override;
};
} // namespace openpuzzle
