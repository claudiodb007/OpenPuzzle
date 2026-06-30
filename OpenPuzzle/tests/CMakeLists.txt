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
