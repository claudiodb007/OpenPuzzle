cmake_minimum_required(VERSION 3.22)

project(OpenPuzzle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

set(
    OPENPUZZLE_BUNDLED_BACKEND
    "opencl"
    CACHE STRING
    "Bundled BitCrack backend: opencl or cuda"
)

set_property(
    CACHE OPENPUZZLE_BUNDLED_BACKEND
    PROPERTY STRINGS opencl cuda
)

if(NOT
   OPENPUZZLE_BUNDLED_BACKEND STREQUAL "opencl" AND
   NOT OPENPUZZLE_BUNDLED_BACKEND STREQUAL "cuda")
    message(FATAL_ERROR
        "OPENPUZZLE_BUNDLED_BACKEND must be opencl or cuda"
    )
endif()

set(
    OPENPUZZLE_BUNDLED_ENGINE
    ""
    CACHE FILEPATH
    "Vetted OpenPuzzle-BitCrack executable"
)

if(OPENPUZZLE_BUNDLED_ENGINE STREQUAL "")
    if(OPENPUZZLE_BUNDLED_BACKEND STREQUAL "opencl")
        set(_openpuzzle_default_engine
            "${CMAKE_SOURCE_DIR}/../OpenPuzzle-BitCrack/bin/clBitCrack"
        )
    else()
        set(_openpuzzle_default_engine
            "${CMAKE_SOURCE_DIR}/../OpenPuzzle-BitCrack/bin/cuBitCrack"
        )
    endif()

    set(
        OPENPUZZLE_BUNDLED_ENGINE
        "${_openpuzzle_default_engine}"
        CACHE FILEPATH
        "Vetted OpenPuzzle-BitCrack executable"
        FORCE
    )
endif()

if(NOT EXISTS "${OPENPUZZLE_BUNDLED_ENGINE}")
    message(FATAL_ERROR
        "Bundled OpenPuzzle-BitCrack executable not found: "
        "${OPENPUZZLE_BUNDLED_ENGINE}"
    )
endif()

get_filename_component(
    OPENPUZZLE_BUNDLED_ENGINE_DIRECTORY
    "${OPENPUZZLE_BUNDLED_ENGINE}"
    DIRECTORY
)

get_filename_component(
    OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY
    "${OPENPUZZLE_BUNDLED_ENGINE_DIRECTORY}"
    DIRECTORY
)

if(NOT EXISTS
   "${OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY}/LICENSE.MIT")
    message(FATAL_ERROR
        "OpenPuzzle-BitCrack LICENSE was not found: "
        "${OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY}/LICENSE.MIT"
    )
endif()

if(OPENPUZZLE_BUNDLED_BACKEND STREQUAL "opencl")
    set(OPENPUZZLE_BUNDLED_ENGINE_NAME "clBitCrack")
    set(OPENPUZZLE_BUNDLED_BACKEND_LABEL "OpenCL")
    set(OPENPUZZLE_BUNDLED_ENGINE_IDENTITY
        "OpenPuzzle-BitCrack 1.0.0 OpenCL protocol 1"
    )
else()
    set(OPENPUZZLE_BUNDLED_ENGINE_NAME "cuBitCrack")
    set(OPENPUZZLE_BUNDLED_BACKEND_LABEL "CUDA")
    set(OPENPUZZLE_BUNDLED_ENGINE_IDENTITY
        "OpenPuzzle-BitCrack 1.0.0 CUDA protocol 1"
    )
endif()

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
        OPENPUZZLE_BUNDLED_BACKEND="${OPENPUZZLE_BUNDLED_BACKEND}"
        OPENPUZZLE_BUNDLED_ENGINE_NAME="${OPENPUZZLE_BUNDLED_ENGINE_NAME}"
        OPENPUZZLE_BUNDLED_ENGINE_IDENTITY="${OPENPUZZLE_BUNDLED_ENGINE_IDENTITY}"
)

set(OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY
    "${CMAKE_BINARY_DIR}/libexec/OpenPuzzle"
)

set(OPENPUZZLE_BUNDLED_ENGINE_BUILD_PATH
    "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY}/${OPENPUZZLE_BUNDLED_ENGINE_NAME}"
)

add_custom_command(
    OUTPUT "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_PATH}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENPUZZLE_BUNDLED_ENGINE}"
        "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_PATH}"
    DEPENDS "${OPENPUZZLE_BUNDLED_ENGINE}"
    VERBATIM
)

add_custom_target(
    OpenPuzzleBundledEngine ALL
    DEPENDS "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_PATH}"
)

add_executable(OpenPuzzle
    src/main.cpp
)

target_link_libraries(OpenPuzzle
    PRIVATE
        OpenPuzzleCore
)

add_dependencies(OpenPuzzle OpenPuzzleBundledEngine)

include(CTest)
enable_testing()

add_subdirectory(tests)


# OpenPuzzle client installation and packaging
include(GNUInstallDirs)

set(
    CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE
)

install(
    TARGETS OpenPuzzle
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    PROGRAMS
        "${OPENPUZZLE_BUNDLED_ENGINE}"
    DESTINATION
        "${CMAKE_INSTALL_LIBEXECDIR}/OpenPuzzle"
    RENAME
        "${OPENPUZZLE_BUNDLED_ENGINE_NAME}"
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

install(
    FILES
        docs/BUNDLED_ENGINE.md
    DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/docs
)

install(
    FILES
        "${OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY}/LICENSE.MIT"
    DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/third-party/OpenPuzzle-BitCrack
    RENAME
        LICENSE
)

install(
    FILES
        "${OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY}/OPENPUZZLE_PATCHES.md"
    DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/third-party/OpenPuzzle-BitCrack
)

set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_PACKAGE_NAME
    "OpenPuzzle-${OPENPUZZLE_BUNDLED_BACKEND_LABEL}"
)
set(CPACK_DEBIAN_PACKAGE_NAME
    "openpuzzle-${OPENPUZZLE_BUNDLED_BACKEND}"
)

if(OPENPUZZLE_BUNDLED_BACKEND STREQUAL "opencl")
    set(OPENPUZZLE_OTHER_BACKEND "cuda")
else()
    set(OPENPUZZLE_OTHER_BACKEND "opencl")
endif()

set(
    CPACK_DEBIAN_PACKAGE_CONFLICTS
    "openpuzzle, openpuzzle-${OPENPUZZLE_OTHER_BACKEND}"
)

set(
    CPACK_DEBIAN_PACKAGE_REPLACES
    "openpuzzle"
)

set(
    CPACK_DEBIAN_PACKAGE_PROVIDES
    "openpuzzle"
)

# Debian/Ubuntu package metadata.
#
# curl is currently used by the HTTP client process.
# ca-certificates validates the HTTPS connection.
# Shared library dependencies are detected by dpkg-shlibdeps.
if(OPENPUZZLE_BUNDLED_BACKEND STREQUAL "opencl")
    set(
        CPACK_DEBIAN_PACKAGE_DEPENDS
        "curl, ca-certificates, ocl-icd-libopencl1"
    )
else()
    set(
        CPACK_DEBIAN_PACKAGE_DEPENDS
        "curl, ca-certificates"
    )
endif()
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
    "OpenPuzzle-${PROJECT_VERSION}-${OPENPUZZLE_BUNDLED_BACKEND_LABEL}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(
    CPACK_PACKAGE_DIRECTORY
    "${CMAKE_SOURCE_DIR}/dist"
)
set(CPACK_PACKAGE_CHECKSUM "SHA256")

include(CPack)
