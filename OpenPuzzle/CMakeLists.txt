cmake_minimum_required(VERSION 3.22)

project(OpenPuzzle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

add_library(OpenPuzzleCore
    src/config/ConfigurationManager.cpp
    src/app/ApplicationContext.cpp
    src/runtime/RuntimeManager.cpp
    src/runtime/DaemonRunner.cpp
    src/runtime/DaemonStatusCollector.cpp
    src/runtime/SchedulerTick.cpp
    src/scheduler/CapabilityScheduler.cpp
    src/scheduler/DefaultSchedulingPolicy.cpp
    src/runtime/RuntimeDispatcher.cpp
    src/runtime/RuntimeCoordinator.cpp
    src/runtime/ExecutionPlanner.cpp
    src/runtime/ExecutionRequestBuilder.cpp
    src/runtime/LocalExecutionBackend.cpp
    src/runtime/ExecutionResourceFactory.cpp
    src/runtime/ExecutionRegistry.cpp
    src/runtime/ExecutionLauncher.cpp
    src/runtime/ExecutionMonitor.cpp
    src/runtime/BackgroundExecutionLauncher.cpp
    src/runtime/ExecutionStopper.cpp
    src/runtime/Execution.cpp
    src/runtime/ExecutionRepository.cpp
    src/runtime/ExecutionPersistence.cpp
    src/runtime/ExecutionProcessMonitor.cpp
    src/workers/Worker.cpp
    src/workers/WorkerAgent.cpp
    src/workers/WorkerAgentFactory.cpp
    src/workers/WorkerAgentRegistry.cpp
    src/workers/WorkerLifecycle.cpp
    src/workers/WorkerRuntime.cpp
    src/workers/WorkerRegistry.cpp
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
    src/core/commands/RangeCommand.cpp
    src/runtime/RunSession.cpp
    src/runtime/ClientRuntime.cpp
    src/runtime/ClientRuntimeControl.cpp
    src/runtime/WorkspaceSecurity.cpp
    src/setup/FirstRunSetup.cpp
    src/client/HttpRangeClient.cpp
    src/client/ClientHeartbeatService.cpp
    src/client/ClientIdentity.cpp
    src/client/ClientRegistrationService.cpp
    src/client/ClientStateStore.cpp
    src/client/ExecutionSyncService.cpp
    src/core/commands/CommandRegistry.cpp
    src/core/commands/StartJobCommand.cpp
    src/services/PuzzleService.cpp
    src/services/QueueService.cpp
    src/services/ServiceUtils.cpp
    src/services/BenchmarkService.cpp
    src/services/SchedulerService.cpp
    src/services/EngineService.cpp
    src/services/ExecutionService.cpp
    src/services/DoctorService.cpp
    src/services/DashboardService.cpp
    src/services/DaemonService.cpp
    src/services/DispatchService.cpp
    src/services/WorkerService.cpp
    src/services/WorkerRunService.cpp
    src/services/HeartbeatService.cpp
    src/engines/bitcrack/BitCrackEngine.cpp
    src/engines/keyhunt/KeyHuntEngine.cpp
    src/engines/EngineLaunchBuilder.cpp
    src/engines/EngineParserFactory.cpp
    src/engines/EngineManager.cpp
    src/engines/common/EngineRegistry.cpp
    src/engines/common/EngineFactory.cpp
    src/engines/common/EngineDiscovery.cpp
    src/database/Database.cpp
    src/models/KeySpace.cpp
    src/allocator/RangeAllocator.cpp
    src/tools/ToolManager.cpp
    src/hardware/GpuManager.cpp
    src/adapters/bitcrack/BitCrackOutputParser.cpp
    src/adapters/bitcrack/BitCrackProgressParser.cpp
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


target_compile_definitions(OpenPuzzleCore
    PUBLIC
        OPENPUZZLE_VERSION="${PROJECT_VERSION}"
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


# OpenPuzzle client installation and packaging
include(GNUInstallDirs)

install(
    TARGETS OpenPuzzle
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES
        README.md
        CHANGELOG.md
        LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(
    FILES
        docs/CLIENT_RUNTIME.md
        docs/RELEASE_1.0.0.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
)

set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_PACKAGE_NAME "OpenPuzzle")

# Debian/Ubuntu package metadata.
#
# curl is currently used by the HTTP client process.
# ca-certificates validates the HTTPS connection.
# Shared library dependencies are detected by dpkg-shlibdeps.
set(
    CPACK_DEBIAN_PACKAGE_DEPENDS
    "curl, ca-certificates"
)
set(
    CPACK_DEBIAN_PACKAGE_MAINTAINER
    "OpenPuzzle Project"
)
set(
    CPACK_DEBIAN_PACKAGE_HOMEPAGE
    "https://claudiodb.com"
)
set(
    CPACK_DEBIAN_PACKAGE_SECTION
    "utils"
)
set(
    CPACK_DEBIAN_PACKAGE_PRIORITY
    "optional"
)
set(
    CPACK_DEBIAN_PACKAGE_SHLIBDEPS
    ON
)
set(CPACK_PACKAGE_VENDOR "OpenPuzzle")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(
    CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Continuous OpenPuzzle client"
)
set(
    CPACK_PACKAGE_FILE_NAME
    "OpenPuzzle-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(
    CPACK_PACKAGE_DIRECTORY
    "${CMAKE_SOURCE_DIR}/dist"
)
set(CPACK_PACKAGE_CHECKSUM "SHA256")

include(CPack)
