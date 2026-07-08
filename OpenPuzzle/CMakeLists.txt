cmake_minimum_required(VERSION 3.22)

project(OpenPuzzle VERSION 0.10.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

add_library(OpenPuzzleCore
    src/config/ConfigurationManager.cpp
    src/app/ApplicationContext.cpp
    src/runtime/RuntimeManager.cpp
    src/runtime/ExecutionRegistry.cpp
    src/runtime/ExecutionLauncher.cpp
    src/runtime/Execution.cpp
    src/core/ProcessRunner.cpp
    src/core/PosixProcessRunner.cpp
    src/core/ProcessRunnerFactory.cpp
    src/core/ExecutionManager.cpp
    src/core/ExecutionSession.cpp
    src/core/ExecutionRecord.cpp
    src/core/ManagedExecutionSession.cpp
    src/core/EventBus.cpp
    src/core/Scheduler.cpp
    src/core/WorkspaceManager.cpp
    src/core/CommandContext.cpp
    src/core/SignalHandler.cpp
    src/dispatcher/Dispatcher.cpp
    src/dispatcher/WorkerSelector.cpp
    src/dispatcher/ProfileSelector.cpp
    src/core/RecoveryManager.cpp
    src/core/Application.cpp
    src/core/commands/ApplicationTestCommands.cpp
    src/core/commands/ProfileCommand.cpp
    src/core/commands/BenchmarkCommand.cpp
    src/core/commands/DispatchCommand.cpp
    src/core/commands/WorkerRunCommand.cpp
    src/core/commands/CommandRegistry.cpp
    src/core/commands/StartJobCommand.cpp
    src/services/PuzzleService.cpp
    src/services/QueueService.cpp
    src/services/ServiceUtils.cpp
    src/services/BenchmarkService.cpp
    src/services/SchedulerService.cpp
    src/services/EngineService.cpp
    src/services/DoctorService.cpp
    src/services/DashboardService.cpp
    src/services/DispatchService.cpp
    src/services/WorkerService.cpp
    src/services/WorkerRunService.cpp
    src/services/HeartbeatService.cpp
    src/engines/bitcrack/BitCrackEngine.cpp
    src/engines/keyhunt/KeyHuntEngine.cpp
    src/engines/EngineManager.cpp
    src/engines/EngineMonitor.cpp
    src/engines/common/EngineRegistry.cpp
    src/engines/common/EngineFactory.cpp
    src/engines/common/EngineDiscovery.cpp
    src/database/Database.cpp
    src/models/KeySpace.cpp
    src/allocator/RangeAllocator.cpp
    src/tools/ToolManager.cpp
    src/hardware/GpuManager.cpp
    src/adapters/bitcrack/BitCrackOutputParser.cpp
    src/performance/AutoTuner.cpp
src/performance/BenchmarkRunner.cpp
    src/performance/GpuProfileManager.cpp
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
