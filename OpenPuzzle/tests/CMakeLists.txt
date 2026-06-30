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
