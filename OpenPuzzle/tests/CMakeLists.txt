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
