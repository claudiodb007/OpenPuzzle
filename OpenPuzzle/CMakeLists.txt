cmake_minimum_required(VERSION 3.22)

project(OpenPuzzle VERSION 1.0.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

set(
    OPENPUZZLE_BUNDLED_CUDA_ENGINE
    "${CMAKE_SOURCE_DIR}/../OpenPuzzle-BitCrack/bin/cuBitCrack"
    CACHE FILEPATH
    "Vetted OpenPuzzle-BitCrack CUDA executable"
)

set(
    OPENPUZZLE_BUNDLED_OPENCL_ENGINE
    "${CMAKE_SOURCE_DIR}/../OpenPuzzle-BitCrack/bin/clBitCrack"
    CACHE FILEPATH
    "Vetted OpenPuzzle-BitCrack OpenCL executable"
)

set(
    OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE
    "${CMAKE_SOURCE_DIR}/../../cpu-engine-lab/keyhunt-portable/keyhunt"
    CACHE FILEPATH
    "Vetted KeyHunt CPU executable"
)

foreach(_openpuzzle_engine
        "${OPENPUZZLE_BUNDLED_CUDA_ENGINE}"
        "${OPENPUZZLE_BUNDLED_OPENCL_ENGINE}")
    if(NOT EXISTS "${_openpuzzle_engine}")
        message(FATAL_ERROR
            "Bundled OpenPuzzle-BitCrack executable not found: "
            "${_openpuzzle_engine}"
        )
    endif()
endforeach()

if(NOT EXISTS "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}")
    message(FATAL_ERROR
        "Bundled KeyHunt executable not found: "
        "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}"
    )
endif()

set(
    OPENPUZZLE_EXPECTED_KEYHUNT_SHA256
    "ac31af4f8713b6cb6056973d16ba62b56eeae9588c3208a9714f3cd236287519"
)

file(
    SHA256
    "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}"
    OPENPUZZLE_BUNDLED_KEYHUNT_SHA256
)

if(NOT
   "${OPENPUZZLE_BUNDLED_KEYHUNT_SHA256}"
   STREQUAL
   "${OPENPUZZLE_EXPECTED_KEYHUNT_SHA256}")
    message(FATAL_ERROR
        "Bundled KeyHunt checksum mismatch: "
        "${OPENPUZZLE_BUNDLED_KEYHUNT_SHA256}"
    )
endif()

get_filename_component(
    OPENPUZZLE_KEYHUNT_SOURCE_DIRECTORY
    "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}"
    DIRECTORY
)

if(NOT EXISTS
   "${OPENPUZZLE_KEYHUNT_SOURCE_DIRECTORY}/LICENSE")
    message(FATAL_ERROR
        "KeyHunt LICENSE was not found: "
        "${OPENPUZZLE_KEYHUNT_SOURCE_DIRECTORY}/LICENSE"
    )
endif()

get_filename_component(
    OPENPUZZLE_BITCRACK_BIN_DIRECTORY
    "${OPENPUZZLE_BUNDLED_OPENCL_ENGINE}"
    DIRECTORY
)

get_filename_component(
    OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY
    "${OPENPUZZLE_BITCRACK_BIN_DIRECTORY}"
    DIRECTORY
)

if(NOT EXISTS
   "${OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY}/LICENSE.MIT")
    message(FATAL_ERROR
        "OpenPuzzle-BitCrack LICENSE was not found: "
        "${OPENPUZZLE_BITCRACK_SOURCE_DIRECTORY}/LICENSE.MIT"
    )
endif()

set(OPENPUZZLE_CUDA_ENGINE_NAME "cuBitCrack")
set(OPENPUZZLE_OPENCL_ENGINE_NAME "clBitCrack")
set(OPENPUZZLE_KEYHUNT_ENGINE_NAME "keyhunt")

set(OPENPUZZLE_CUDA_ENGINE_IDENTITY
    "OpenPuzzle-BitCrack 1.0.0 CUDA protocol 2"
)

set(OPENPUZZLE_OPENCL_ENGINE_IDENTITY
    "OpenPuzzle-BitCrack 1.0.0 OpenCL protocol 2"
)

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
    src/runtime/RunBenchmarkPreparation.cpp
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
    src/client/SolutionExporter.cpp
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
    src/adapters/keyhunt/KeyHuntProgressParser.cpp
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
        OPENPUZZLE_CUDA_ENGINE_NAME="${OPENPUZZLE_CUDA_ENGINE_NAME}"
        OPENPUZZLE_OPENCL_ENGINE_NAME="${OPENPUZZLE_OPENCL_ENGINE_NAME}"
        OPENPUZZLE_KEYHUNT_ENGINE_NAME="${OPENPUZZLE_KEYHUNT_ENGINE_NAME}"
        OPENPUZZLE_CUDA_ENGINE_IDENTITY="${OPENPUZZLE_CUDA_ENGINE_IDENTITY}"
        OPENPUZZLE_OPENCL_ENGINE_IDENTITY="${OPENPUZZLE_OPENCL_ENGINE_IDENTITY}"
)

set(OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY
    "${CMAKE_BINARY_DIR}/libexec/OpenPuzzle"
)

set(OPENPUZZLE_CUDA_ENGINE_BUILD_PATH
    "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY}/${OPENPUZZLE_CUDA_ENGINE_NAME}"
)

set(OPENPUZZLE_OPENCL_ENGINE_BUILD_PATH
    "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY}/${OPENPUZZLE_OPENCL_ENGINE_NAME}"
)

set(OPENPUZZLE_KEYHUNT_ENGINE_BUILD_PATH
    "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY}/${OPENPUZZLE_KEYHUNT_ENGINE_NAME}"
)

add_custom_command(
    OUTPUT
        "${OPENPUZZLE_CUDA_ENGINE_BUILD_PATH}"
        "${OPENPUZZLE_OPENCL_ENGINE_BUILD_PATH}"
        "${OPENPUZZLE_KEYHUNT_ENGINE_BUILD_PATH}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${OPENPUZZLE_BUNDLED_ENGINE_BUILD_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENPUZZLE_BUNDLED_CUDA_ENGINE}"
        "${OPENPUZZLE_CUDA_ENGINE_BUILD_PATH}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENPUZZLE_BUNDLED_OPENCL_ENGINE}"
        "${OPENPUZZLE_OPENCL_ENGINE_BUILD_PATH}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}"
        "${OPENPUZZLE_KEYHUNT_ENGINE_BUILD_PATH}"
    DEPENDS
        "${OPENPUZZLE_BUNDLED_CUDA_ENGINE}"
        "${OPENPUZZLE_BUNDLED_OPENCL_ENGINE}"
        "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}"
    VERBATIM
)

add_custom_target(
    OpenPuzzleBundledEngines ALL
    DEPENDS
        "${OPENPUZZLE_CUDA_ENGINE_BUILD_PATH}"
        "${OPENPUZZLE_OPENCL_ENGINE_BUILD_PATH}"
        "${OPENPUZZLE_KEYHUNT_ENGINE_BUILD_PATH}"
)

add_executable(OpenPuzzle
    src/main.cpp
)

# Linux commands conventionally use lowercase names.
# The CMake target remains OpenPuzzle for internal compatibility.
set_target_properties(OpenPuzzle
    PROPERTIES
        OUTPUT_NAME openpuzzle
)

target_link_libraries(OpenPuzzle
    PRIVATE
        OpenPuzzleCore
)

add_dependencies(OpenPuzzle OpenPuzzleBundledEngines)

include(CTest)
enable_testing()

add_subdirectory(tests)

add_test(
    NAME SystemdServiceUnitTests
    COMMAND
        ${CMAKE_COMMAND}
        "-DOPENPUZZLE_SERVICE_FILE=${CMAKE_SOURCE_DIR}/packaging/systemd/openpuzzle@.service"
        -P
            ${CMAKE_SOURCE_DIR}/tests/cmake/SystemdServiceUnitTests.cmake
)


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
    FILES
        packaging/systemd/openpuzzle@.service
    DESTINATION
        lib/systemd/user
)

install(
    PROGRAMS
        "${OPENPUZZLE_BUNDLED_CUDA_ENGINE}"
        "${OPENPUZZLE_BUNDLED_OPENCL_ENGINE}"
        "${OPENPUZZLE_BUNDLED_KEYHUNT_ENGINE}"
    DESTINATION
        "${CMAKE_INSTALL_LIBEXECDIR}/OpenPuzzle"
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
        docs/RELEASE_1.0.1.md
        docs/RELEASE_1.0.2.md
        docs/RELEASE_1.0.3.md
        docs/SYSTEMD_SERVICE.md
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

install(
    FILES
        "${OPENPUZZLE_KEYHUNT_SOURCE_DIRECTORY}/LICENSE"
    DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/third-party/KeyHunt
)

# One public package contains both vetted GPU backends.
# CUDA is preferred when a working CUDA device is available;
# otherwise OpenCL is selected automatically.
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_PACKAGE_NAME "OpenPuzzle")
set(CPACK_DEBIAN_PACKAGE_NAME "openpuzzle")

set(
    CPACK_DEBIAN_PACKAGE_CONFLICTS
    "openpuzzle-cuda, openpuzzle-opencl"
)

set(
    CPACK_DEBIAN_PACKAGE_REPLACES
    "openpuzzle-cuda, openpuzzle-opencl"
)

set(
    CPACK_DEBIAN_PACKAGE_PROVIDES
    "openpuzzle"
)

# curl performs the current HTTPS transport, ca-certificates
# validates the server certificate and the OpenCL ICD loader is
# required by the bundled OpenCL engine. CUDA is supplied by the
# installed NVIDIA driver when that backend is selected.
set(
    CPACK_DEBIAN_PACKAGE_DEPENDS
    "curl, ca-certificates, ocl-icd-libopencl1"
)

set(
    CPACK_DEBIAN_PACKAGE_MAINTAINER
    "OpenPuzzle Project"
)
set(
    CPACK_DEBIAN_PACKAGE_HOMEPAGE
    "https://claudiodb.com"
)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_PACKAGE_VENDOR "OpenPuzzle")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(
    CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Continuous OpenPuzzle client with automatic GPU backend selection"
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
