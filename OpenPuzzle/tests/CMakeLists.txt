add_executable(EventBusTests
    core/EventBusTests.cpp
)

target_link_libraries(EventBusTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME EventBusTests
    COMMAND EventBusTests
)

add_executable(ProcessRunnerTests
    core/ProcessRunnerTests.cpp
)

target_link_libraries(ProcessRunnerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ProcessRunnerTests
    COMMAND ProcessRunnerTests
)

add_executable(BitCrackParserTests
    adapters/BitCrackParserTests.cpp
)

target_link_libraries(BitCrackParserTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BitCrackParserTests
    COMMAND BitCrackParserTests
)

add_executable(ExecutionSessionTests
    core/ExecutionSessionTests.cpp
)

target_link_libraries(ExecutionSessionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionSessionTests
    COMMAND ExecutionSessionTests
)

add_executable(ExecutionManagerTests
    core/ExecutionManagerTests.cpp
)

target_link_libraries(ExecutionManagerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionManagerTests
    COMMAND ExecutionManagerTests
)

add_executable(ExecutionContextTests
    core/ExecutionContextTests.cpp
)

target_link_libraries(ExecutionContextTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextTests
    COMMAND ExecutionContextTests
)

add_executable(ExecutionResultTests
    core/ExecutionResultTests.cpp
)

target_link_libraries(ExecutionResultTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionResultTests
    COMMAND ExecutionResultTests
)

add_executable(ExecutionManagerContextTests
    core/ExecutionManagerContextTests.cpp
)

target_link_libraries(ExecutionManagerContextTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionManagerContextTests
    COMMAND ExecutionManagerContextTests
)

add_executable(SchedulerTests
    core/SchedulerTests.cpp
)

target_link_libraries(SchedulerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerTests
    COMMAND SchedulerTests
)

add_executable(SchedulerEventTests
    core/SchedulerEventTests.cpp
)

target_link_libraries(SchedulerEventTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerEventTests
    COMMAND SchedulerEventTests
)

add_executable(WorkspaceManagerTests
    core/WorkspaceManagerTests.cpp
)

target_link_libraries(WorkspaceManagerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkspaceManagerTests
    COMMAND WorkspaceManagerTests
)

add_executable(ExecutionWorkspaceTests
    integration/ExecutionWorkspaceTests.cpp
)

target_link_libraries(ExecutionWorkspaceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionWorkspaceTests
    COMMAND ExecutionWorkspaceTests
)


add_executable(RecoveryManagerTests
    core/RecoveryManagerTests.cpp
)

target_link_libraries(RecoveryManagerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME RecoveryManagerTests
    COMMAND RecoveryManagerTests
)

add_executable(RecoveryStateTests
    core/RecoveryStateTests.cpp
)

target_link_libraries(RecoveryStateTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME RecoveryStateTests
    COMMAND RecoveryStateTests
)

add_executable(ResumeCommandTests
    integration/ResumeCommandTests.cpp
)

add_dependencies(ResumeCommandTests
    OpenPuzzle
)

add_test(
    NAME ResumeCommandTests
    COMMAND ResumeCommandTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(ResumeCommandMissingStateTests
    integration/ResumeCommandMissingStateTests.cpp
)

add_dependencies(ResumeCommandMissingStateTests
    OpenPuzzle
)

add_test(
    NAME ResumeCommandMissingStateTests
    COMMAND ResumeCommandMissingStateTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(ResumeLatestCommandTests
    integration/ResumeLatestCommandTests.cpp
)

add_dependencies(ResumeLatestCommandTests
    OpenPuzzle
)

add_test(
    NAME ResumeLatestCommandTests
    COMMAND ResumeLatestCommandTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


add_executable(ExecutionContextRecoveryTests
    core/ExecutionContextRecoveryTests.cpp
)

target_link_libraries(ExecutionContextRecoveryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextRecoveryTests
    COMMAND ExecutionContextRecoveryTests
)


add_executable(ExecutionContextEscapedCommandTests
    core/ExecutionContextEscapedCommandTests.cpp
)

target_link_libraries(ExecutionContextEscapedCommandTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextEscapedCommandTests
    COMMAND ExecutionContextEscapedCommandTests
)

add_executable(ResumeRunCommandTests
    integration/ResumeRunCommandTests.cpp
)

add_dependencies(ResumeRunCommandTests
    OpenPuzzle
)

add_test(
    NAME ResumeRunCommandTests
    COMMAND ResumeRunCommandTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


add_executable(SchedulerExecutionTests
    core/SchedulerExecutionTests.cpp
)

target_link_libraries(SchedulerExecutionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerExecutionTests
    COMMAND SchedulerExecutionTests
)


add_executable(SchedulerDatabaseTests
    core/SchedulerDatabaseTests.cpp
)

target_link_libraries(SchedulerDatabaseTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerDatabaseTests
    COMMAND SchedulerDatabaseTests
)


add_executable(SchedulerContextBuilderTests
    core/SchedulerContextBuilderTests.cpp
)

target_link_libraries(SchedulerContextBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerContextBuilderTests
    COMMAND SchedulerContextBuilderTests
)


add_executable(SchedulerCommandBuilderTests
    core/SchedulerCommandBuilderTests.cpp
)

target_link_libraries(SchedulerCommandBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerCommandBuilderTests
    COMMAND SchedulerCommandBuilderTests
)


add_executable(SchedulerStartJobTests
    core/SchedulerStartJobTests.cpp
)

target_link_libraries(SchedulerStartJobTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerStartJobTests
    COMMAND SchedulerStartJobTests
)


add_executable(BitCrackParserTotalTests
    adapters/BitCrackParserTotalTests.cpp
)

target_link_libraries(BitCrackParserTotalTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BitCrackParserTotalTests
    COMMAND BitCrackParserTotalTests
)


add_executable(ExecutionProgressPersistenceTests
    core/ExecutionProgressPersistenceTests.cpp
)

target_link_libraries(ExecutionProgressPersistenceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionProgressPersistenceTests
    COMMAND ExecutionProgressPersistenceTests
)


add_executable(ExecutionLiveProgressTests
    core/ExecutionLiveProgressTests.cpp
)

target_link_libraries(ExecutionLiveProgressTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionLiveProgressTests
    COMMAND ExecutionLiveProgressTests
)


add_executable(AutoTunerTests
    performance/AutoTunerTests.cpp
)

target_link_libraries(AutoTunerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME AutoTunerTests
    COMMAND AutoTunerTests
)
add_executable(BenchmarkRunnerTests
    performance/BenchmarkRunnerTests.cpp
)

target_link_libraries(BenchmarkRunnerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BenchmarkRunnerTests
    COMMAND BenchmarkRunnerTests
)


add_executable(BenchmarkRunnerExecutionTests
    performance/BenchmarkRunnerExecutionTests.cpp
)

target_link_libraries(BenchmarkRunnerExecutionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BenchmarkRunnerExecutionTests
    COMMAND BenchmarkRunnerExecutionTests
)


add_executable(PosixProcessRunnerTests
    core/PosixProcessRunnerTests.cpp
)

target_link_libraries(PosixProcessRunnerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME PosixProcessRunnerTests
    COMMAND PosixProcessRunnerTests
)


add_executable(PosixProcessRunnerTimeoutTests
    core/PosixProcessRunnerTimeoutTests.cpp
)

target_link_libraries(PosixProcessRunnerTimeoutTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME PosixProcessRunnerTimeoutTests
    COMMAND PosixProcessRunnerTimeoutTests
)
