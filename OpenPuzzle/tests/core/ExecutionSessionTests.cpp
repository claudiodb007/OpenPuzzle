#include "openpuzzle/core/ExecutionSession.hpp"

using namespace openpuzzle;

int main() {
    ExecutionSession session;

    session.executionId = 10;
    session.jobId = 20;
    session.puzzleId = 71;
    session.rangeId = 30;
    session.engine = "BitCrack";
    session.workspace = "workspace";
    session.command = "command";
    session.status = ExecutionSessionStatus::Running;

    if (session.executionId != 10) return 1;
    if (session.jobId != 20) return 2;
    if (session.puzzleId != 71) return 3;
    if (session.rangeId != 30) return 4;
    if (session.engine != "BitCrack") return 5;
    if (session.workspace != "workspace") return 6;
    if (session.command != "command") return 7;
    if (ExecutionSession::statusToString(session.status) != "RUNNING") return 8;

    if (ExecutionSession::statusToString(ExecutionSessionStatus::Created) != "CREATED") return 9;
    if (ExecutionSession::statusToString(ExecutionSessionStatus::Finished) != "FINISHED") return 10;
    if (ExecutionSession::statusToString(ExecutionSessionStatus::Failed) != "FAILED") return 11;
    if (ExecutionSession::statusToString(ExecutionSessionStatus::Cancelled) != "CANCELLED") return 12;

    return 0;
}
