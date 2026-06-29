cmake_minimum_required(VERSION 3.22)
project(OpenPuzzle VERSION 0.10.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(SQLite3 REQUIRED)
add_executable(OpenPuzzle
    src/main.cpp
    src/core/Application.cpp
    src/database/Database.cpp
    src/models/KeySpace.cpp
    src/allocator/RangeAllocator.cpp
    src/tools/ToolManager.cpp
    src/hardware/GpuManager.cpp
)
target_include_directories(OpenPuzzle PRIVATE include)
target_link_libraries(OpenPuzzle PRIVATE SQLite::SQLite3)
target_compile_options(OpenPuzzle PRIVATE -Wall -Wextra -Wpedantic)
