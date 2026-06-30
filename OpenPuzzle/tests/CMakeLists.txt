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
