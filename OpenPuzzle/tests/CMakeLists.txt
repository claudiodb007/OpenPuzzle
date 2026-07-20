# Automated tests use assert() as their
# validation mechanism.
#
# Release builds define NDEBUG globally,
# so explicitly undefine it for test targets.
if(
    CMAKE_CXX_COMPILER_ID MATCHES
    "GNU|Clang"
)
    add_compile_options(
        -UNDEBUG
    )
endif()

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

add_executable(ResumeCommandTests
    integration/ResumeCommandTests.cpp
)

add_dependencies(ResumeCommandTests
    OpenPuzzle
)

add_test(
    NAME ResumeCommandTests
    COMMAND ResumeCommandTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(ResumeCommandMissingStateTests
    integration/ResumeCommandMissingStateTests.cpp
)

add_dependencies(ResumeCommandMissingStateTests
    OpenPuzzle
)

add_test(
    NAME ResumeCommandMissingStateTests
    COMMAND ResumeCommandMissingStateTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(ResumeLatestCommandTests
    integration/ResumeLatestCommandTests.cpp
)

add_dependencies(ResumeLatestCommandTests
    OpenPuzzle
)

add_test(
    NAME ResumeLatestCommandTests
    COMMAND ResumeLatestCommandTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


add_executable(ExecutionContextRecoveryTests
    core/ExecutionContextRecoveryTests.cpp
)

target_link_libraries(ExecutionContextRecoveryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextRecoveryTests
    COMMAND ExecutionContextRecoveryTests
)


add_executable(ExecutionContextEscapedCommandTests
    core/ExecutionContextEscapedCommandTests.cpp
)

target_link_libraries(ExecutionContextEscapedCommandTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextEscapedCommandTests
    COMMAND ExecutionContextEscapedCommandTests
)

add_executable(ResumeRunCommandTests
    integration/ResumeRunCommandTests.cpp
)

add_dependencies(ResumeRunCommandTests
    OpenPuzzle
)

add_test(
    NAME ResumeRunCommandTests
    COMMAND ResumeRunCommandTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


add_executable(SchedulerExecutionTests
    core/SchedulerExecutionTests.cpp
)

target_link_libraries(SchedulerExecutionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerExecutionTests
    COMMAND SchedulerExecutionTests
)


add_executable(SchedulerDatabaseTests
    core/SchedulerDatabaseTests.cpp
)

target_link_libraries(SchedulerDatabaseTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerDatabaseTests
    COMMAND SchedulerDatabaseTests
)


add_executable(SchedulerContextBuilderTests
    core/SchedulerContextBuilderTests.cpp
)

target_link_libraries(SchedulerContextBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerContextBuilderTests
    COMMAND SchedulerContextBuilderTests
)


add_executable(SchedulerCommandBuilderTests
    core/SchedulerCommandBuilderTests.cpp
)

target_link_libraries(SchedulerCommandBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerCommandBuilderTests
    COMMAND SchedulerCommandBuilderTests
)


add_executable(BitCrackEngineCommandTests
    engines/BitCrackEngineCommandTests.cpp
)

target_link_libraries(BitCrackEngineCommandTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BitCrackEngineCommandTests
    COMMAND BitCrackEngineCommandTests
)


add_executable(SchedulerStartJobTests
    core/SchedulerStartJobTests.cpp
)

target_link_libraries(SchedulerStartJobTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerStartJobTests
    COMMAND SchedulerStartJobTests
)


add_executable(BitCrackParserTotalTests
    adapters/BitCrackParserTotalTests.cpp
)

target_link_libraries(BitCrackParserTotalTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BitCrackParserTotalTests
    COMMAND BitCrackParserTotalTests
)


add_executable(ExecutionProgressPersistenceTests
    core/ExecutionProgressPersistenceTests.cpp
)

target_link_libraries(ExecutionProgressPersistenceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionProgressPersistenceTests
    COMMAND ExecutionProgressPersistenceTests
)


add_executable(ExecutionLiveProgressTests
    core/ExecutionLiveProgressTests.cpp
)

target_link_libraries(ExecutionLiveProgressTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionLiveProgressTests
    COMMAND ExecutionLiveProgressTests
)


add_executable(AutoTunerTests
    performance/AutoTunerTests.cpp
)

target_link_libraries(AutoTunerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME AutoTunerTests
    COMMAND AutoTunerTests
)
add_executable(BenchmarkRunnerTests
    performance/BenchmarkRunnerTests.cpp
)

target_link_libraries(BenchmarkRunnerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BenchmarkRunnerTests
    COMMAND BenchmarkRunnerTests
)


add_executable(BenchmarkRunnerExecutionTests
    performance/BenchmarkRunnerExecutionTests.cpp
)

target_link_libraries(BenchmarkRunnerExecutionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BenchmarkRunnerExecutionTests
    COMMAND BenchmarkRunnerExecutionTests
)


add_executable(PosixProcessRunnerTests
    core/PosixProcessRunnerTests.cpp
)

target_link_libraries(PosixProcessRunnerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME PosixProcessRunnerTests
    COMMAND PosixProcessRunnerTests
)


add_executable(PosixProcessRunnerTimeoutTests
    core/PosixProcessRunnerTimeoutTests.cpp
)

target_link_libraries(PosixProcessRunnerTimeoutTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME PosixProcessRunnerTimeoutTests
    COMMAND PosixProcessRunnerTimeoutTests
)


add_executable(ProcessHandleTests
    core/ProcessHandleTests.cpp
)

target_link_libraries(ProcessHandleTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ProcessHandleTests
    COMMAND ProcessHandleTests
)


add_executable(ManagedExecutionSessionTests
    core/ManagedExecutionSessionTests.cpp
)

target_link_libraries(ManagedExecutionSessionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ManagedExecutionSessionTests
    COMMAND ManagedExecutionSessionTests
)


add_executable(ExecutionRecordTests
    core/ExecutionRecordTests.cpp
)

target_link_libraries(ExecutionRecordTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionRecordTests
    COMMAND ExecutionRecordTests
)


add_executable(GpuInfoTests
    core/GpuInfoTests.cpp
)

target_link_libraries(GpuInfoTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME GpuInfoTests
    COMMAND GpuInfoTests
)

add_executable(DispatcherTests
    dispatcher/DispatcherTests.cpp
)

target_link_libraries(DispatcherTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME DispatcherTests
    COMMAND DispatcherTests
)

add_executable(DispatcherExecutionTests
    dispatcher/DispatcherExecutionTests.cpp
)

target_link_libraries(DispatcherExecutionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME DispatcherExecutionTests
    COMMAND DispatcherExecutionTests
)

add_executable(ExecutionStopperTests
    runtime/ExecutionStopperTests.cpp
)

target_link_libraries(ExecutionStopperTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionStopperTests
    COMMAND ExecutionStopperTests
)

add_executable(ExecutionProcessMonitorTests
    runtime/ExecutionProcessMonitorTests.cpp
)

target_link_libraries(ExecutionProcessMonitorTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionProcessMonitorTests
    COMMAND ExecutionProcessMonitorTests
)

add_executable(WorkerAgentTests
    workers/WorkerAgentTests.cpp
)

target_link_libraries(WorkerAgentTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerAgentTests
    COMMAND WorkerAgentTests
)

add_executable(WorkerAgentRegistryTests
    workers/WorkerAgentRegistryTests.cpp
)

target_link_libraries(WorkerAgentRegistryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerAgentRegistryTests
    COMMAND WorkerAgentRegistryTests
)

add_executable(WorkerSelectorTests
    dispatcher/WorkerSelectorTests.cpp
)

target_link_libraries(WorkerSelectorTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerSelectorTests
    COMMAND WorkerSelectorTests
)

add_executable(DefaultSchedulingPolicyTests
    scheduler/DefaultSchedulingPolicyTests.cpp
)

target_link_libraries(DefaultSchedulingPolicyTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME DefaultSchedulingPolicyTests
    COMMAND DefaultSchedulingPolicyTests
)

add_executable(SchedulerPolicyInjectionTests
    scheduler/SchedulerPolicyInjectionTests.cpp
)

target_link_libraries(SchedulerPolicyInjectionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerPolicyInjectionTests
    COMMAND SchedulerPolicyInjectionTests
)

add_executable(WorkerEngineCapabilityTests
    workers/WorkerEngineCapabilityTests.cpp
)

target_link_libraries(WorkerEngineCapabilityTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerEngineCapabilityTests
    COMMAND WorkerEngineCapabilityTests
)

add_executable(WorkerHeartbeatTests
    workers/WorkerHeartbeatTests.cpp
)

target_link_libraries(WorkerHeartbeatTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerHeartbeatTests
    COMMAND WorkerHeartbeatTests
)

add_executable(ExecutionResourceFactoryTests
    runtime/ExecutionResourceFactoryTests.cpp
)

target_link_libraries(ExecutionResourceFactoryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionResourceFactoryTests
    COMMAND ExecutionResourceFactoryTests
)

add_executable(WorkerAgentFactoryTests
    workers/WorkerAgentFactoryTests.cpp
)

target_link_libraries(WorkerAgentFactoryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerAgentFactoryTests
    COMMAND WorkerAgentFactoryTests
)

add_executable(WorkerLifecycleTests
    workers/WorkerLifecycleTests.cpp
)

target_link_libraries(WorkerLifecycleTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerLifecycleTests
    COMMAND WorkerLifecycleTests
)

add_executable(RuntimeCoordinatorTests
    runtime/RuntimeCoordinatorTests.cpp
)

target_link_libraries(RuntimeCoordinatorTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME RuntimeCoordinatorTests
    COMMAND RuntimeCoordinatorTests
)
add_executable(ExecutionRequestBuilderTests
    runtime/ExecutionRequestBuilderTests.cpp
)

target_link_libraries(ExecutionRequestBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionRequestBuilderTests
    COMMAND ExecutionRequestBuilderTests
)

add_executable(EngineLaunchBuilderTests
    engines/EngineLaunchBuilderTests.cpp
)

target_link_libraries(EngineLaunchBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME EngineLaunchBuilderTests
    COMMAND EngineLaunchBuilderTests
)

add_executable(EngineRegistryTests
    engines/EngineRegistryTests.cpp
)

target_link_libraries(EngineRegistryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME EngineRegistryTests
    COMMAND EngineRegistryTests
)

add_executable(EngineManagerTests
    engines/EngineManagerTests.cpp
)

target_link_libraries(EngineManagerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME EngineManagerTests
    COMMAND EngineManagerTests
)

add_executable(KeyHuntEngineCommandTests
    engines/KeyHuntEngineCommandTests.cpp
)

target_link_libraries(KeyHuntEngineCommandTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME KeyHuntEngineCommandTests
    COMMAND KeyHuntEngineCommandTests
)

add_executable(EngineParserFactoryTests
    engines/EngineParserFactoryTests.cpp
)

target_link_libraries(EngineParserFactoryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME EngineParserFactoryTests
    COMMAND EngineParserFactoryTests
)

add_executable(CapabilitySchedulerTests
    scheduler/CapabilitySchedulerTests.cpp
)

target_link_libraries(CapabilitySchedulerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME CapabilitySchedulerTests
    COMMAND CapabilitySchedulerTests
)

add_executable(ExecutionPlannerTests
    runtime/ExecutionPlannerTests.cpp
)

target_link_libraries(
    ExecutionPlannerTests
    PRIVATE
    OpenPuzzleCore
)

add_test(
    NAME ExecutionPlannerTests
    COMMAND ExecutionPlannerTests
)

add_executable(RuntimeDispatcherPlanTests
    runtime/RuntimeDispatcherPlanTests.cpp
)

target_link_libraries(RuntimeDispatcherPlanTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME RuntimeDispatcherPlanTests
    COMMAND RuntimeDispatcherPlanTests
)

add_executable(RuntimeTickResultTests
    runtime/RuntimeTickResultTests.cpp
)

target_link_libraries(RuntimeTickResultTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME RuntimeTickResultTests
    COMMAND RuntimeTickResultTests
)

add_executable(WorkerRuntimeTests
    workers/WorkerRuntimeTests.cpp
)

target_link_libraries(WorkerRuntimeTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkerRuntimeTests
    COMMAND WorkerRuntimeTests
)


add_executable(HttpRangeClientTests
    client/HttpRangeClientTests.cpp
)

target_link_libraries(HttpRangeClientTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME HttpRangeClientTests
    COMMAND HttpRangeClientTests
)


add_executable(ClientStateStoreTests
    client/ClientStateStoreTests.cpp
)

target_link_libraries(ClientStateStoreTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientStateStoreTests
    COMMAND ClientStateStoreTests
)

add_executable(BackgroundExecutionLauncherTests
    runtime/BackgroundExecutionLauncherTests.cpp
)

target_link_libraries(BackgroundExecutionLauncherTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BackgroundExecutionLauncherTests
    COMMAND BackgroundExecutionLauncherTests
)

add_executable(ExecutionSyncServiceTests
    client/ExecutionSyncServiceTests.cpp
)

target_link_libraries(ExecutionSyncServiceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionSyncServiceTests
    COMMAND ExecutionSyncServiceTests
)

add_executable(ClientRegistrationTests
    client/ClientRegistrationTests.cpp
)

target_link_libraries(ClientRegistrationTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientRegistrationTests
    COMMAND ClientRegistrationTests
)

add_executable(ClientRegistrationServiceTests
    client/ClientRegistrationServiceTests.cpp
)

target_link_libraries(ClientRegistrationServiceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientRegistrationServiceTests
    COMMAND ClientRegistrationServiceTests
)

add_executable(ClientHeartbeatTests
    client/ClientHeartbeatTests.cpp
)

target_link_libraries(ClientHeartbeatTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientHeartbeatTests
    COMMAND ClientHeartbeatTests
)

add_executable(ClientHeartbeatServiceTests
    client/ClientHeartbeatServiceTests.cpp
)

target_link_libraries(ClientHeartbeatServiceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientHeartbeatServiceTests
    COMMAND ClientHeartbeatServiceTests
)

add_executable(ClientRuntimeTests
    runtime/ClientRuntimeTests.cpp
)

target_link_libraries(ClientRuntimeTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientRuntimeTests
    COMMAND ClientRuntimeTests
)

add_executable(ClientRuntimeControlTests
    runtime/ClientRuntimeControlTests.cpp
)

target_link_libraries(ClientRuntimeControlTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ClientRuntimeControlTests
    COMMAND ClientRuntimeControlTests
)



find_program(
    OPENPUZZLE_CURL_EXECUTABLE
    curl
)

if(OPENPUZZLE_CURL_EXECUTABLE)
    add_executable(
        HttpRangeClientIntegrationTests
        client/HttpRangeClientIntegrationTests.cpp
    )

    target_link_libraries(
        HttpRangeClientIntegrationTests
        PRIVATE
            OpenPuzzleCore
    )

    add_test(
        NAME HttpRangeClientIntegrationTests
        COMMAND HttpRangeClientIntegrationTests
    )
endif()

add_executable(
    WorkspaceSecurityTests
    runtime/WorkspaceSecurityTests.cpp
)

target_link_libraries(
    WorkspaceSecurityTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME WorkspaceSecurityTests
    COMMAND WorkspaceSecurityTests
)


add_executable(
    LocalStorageSecurityTests
    runtime/LocalStorageSecurityTests.cpp
)

target_link_libraries(
    LocalStorageSecurityTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME LocalStorageSecurityTests
    COMMAND LocalStorageSecurityTests
)





add_executable(
    ApplicationCliTests
    core/ApplicationCliTests.cpp
)

target_link_libraries(
    ApplicationCliTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ApplicationCliTests
    COMMAND ApplicationCliTests
)


add_executable(GpuManagerTests
    hardware/GpuManagerTests.cpp
)

target_link_libraries(GpuManagerTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME GpuManagerTests
    COMMAND GpuManagerTests
)

add_executable(
    ToolManagerTests
    tools/ToolManagerTests.cpp
)

target_link_libraries(
    ToolManagerTests
    PRIVATE
        OpenPuzzleCore
)

add_dependencies(
    ToolManagerTests
    OpenPuzzleBundledEngines
)

add_test(
    NAME ToolManagerTests
    COMMAND ToolManagerTests
)


add_executable(InstallCommandTests
    core/InstallCommandTests.cpp
)

target_link_libraries(InstallCommandTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME InstallCommandTests
    COMMAND InstallCommandTests
)
