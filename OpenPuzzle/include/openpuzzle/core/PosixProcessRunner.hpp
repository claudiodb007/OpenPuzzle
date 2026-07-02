#pragma once

#include "openpuzzle/core/IProcessRunner.hpp"

namespace openpuzzle {

class PosixProcessRunner : public IProcessRunner {
public:
  ProcessResult run(const std::string &command, const LineCallback &onLine,
                    int maxSeconds = 0) const override;
};

} // namespace openpuzzle
