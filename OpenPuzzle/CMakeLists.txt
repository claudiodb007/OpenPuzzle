cmake_minimum_required(VERSION 3.22)

project(OpenPuzzle VERSION 0.10.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

add_library(OpenPuzzleCore
    src/core/ProcessRunner.cpp
    src/core/ExecutionManager.cpp
    src/core/ExecutionSession.cpp
    src/core/EventBus.cpp
    src/core/Scheduler.cpp
    src/core/Application.cpp
    src/core/commands/ApplicationTestCommands.cpp
    src/database/Database.cpp
    src/models/KeySpace.cpp
    src/allocator/RangeAllocator.cpp
    src/tools/ToolManager.cpp
    src/hardware/GpuManager.cpp
    src/adapters/bitcrack/BitCrackOutputParser.cpp
)

target_include_directories(OpenPuzzleCore
    PUBLIC
        include
)

target_link_libraries(OpenPuzzleCore
    PUBLIC
        SQLite::SQLite3
)

target_compile_options(OpenPuzzleCore
    PRIVATE
        -Wall -Wextra -Wpedantic
)

add_executable(OpenPuzzle
    src/main.cpp
)

target_link_libraries(OpenPuzzle
    PRIVATE
        OpenPuzzleCore
)

include(CTest)
enable_testing()

add_subdirectory(tests)
