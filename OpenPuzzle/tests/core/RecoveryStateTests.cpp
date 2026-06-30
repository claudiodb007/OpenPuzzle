#include "openpuzzle/core/RecoveryState.hpp"

using namespace openpuzzle;

int main()
{
    RecoveryState s;

    if (s.executionId != 0) return 1;
    if (s.jobId != 0) return 2;
    if (s.rangeId != 0) return 3;
    if (s.status != RecoveryStatus::Unknown) return 4;
    if (s.exitCode != -1) return 5;

    return 0;
}
