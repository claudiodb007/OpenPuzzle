if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Vanity preview currently requires Linux")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
target_sources(OpenPuzzleCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/VanityCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/VanityPreviewEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/VanityCommonEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LocalAssignmentMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpAssignmentMain.cpp)
target_include_directories(OpenPuzzleCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(OpenPuzzleCore PRIVATE OPENPUZZLE_ENABLE_VANITY_PREVIEW=1)

add_executable(OpenPuzzleVanityAdapter VanityRangePreviewMain.cpp ../../src/core/PosixProcessRunner.cpp)
target_include_directories(OpenPuzzleVanityAdapter PRIVATE ../../include)
set_target_properties(OpenPuzzleVanityAdapter PROPERTIES OUTPUT_NAME openpuzzle-vanity-adapter RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_dependencies(OpenPuzzle OpenPuzzleVanityAdapter)

add_executable(VanityPreviewModelTests tests/ModelTests.cpp)
target_include_directories(VanityPreviewModelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VanityPreviewModelTests PRIVATE OpenPuzzleCore)
add_test(NAME VanityPreviewModelTests COMMAND VanityPreviewModelTests)

add_executable(VanityPreviewProtocolTests tests/ProtocolTests.cpp)
target_include_directories(VanityPreviewProtocolTests PRIVATE ../../include)
add_test(NAME VanityPreviewProtocolTests COMMAND VanityPreviewProtocolTests)

add_test(NAME VanityPreviewAdapterTests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ClientTests.py $<TARGET_FILE:OpenPuzzleVanityAdapter>)
add_test(NAME VanityPreviewCliTests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/CliTests.py $<TARGET_FILE:OpenPuzzle> $<TARGET_FILE:OpenPuzzleVanityAdapter>)
set_tests_properties(VanityPreviewAdapterTests VanityPreviewCliTests PROPERTIES TIMEOUT 180)

add_test(NAME VanityPreviewHttpTests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/HttpTests.py $<TARGET_FILE:OpenPuzzle> $<TARGET_FILE:OpenPuzzleVanityAdapter>)
set_tests_properties(VanityPreviewHttpTests PROPERTIES TIMEOUT 180)

target_compile_definitions(OpenPuzzleCore PRIVATE
 OPENPUZZLE_VANITY_PYTHON="${Python3_EXECUTABLE}"
 OPENPUZZLE_VANITY_MEASURED_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/measured/Profile.py")
add_test(NAME VanityMeasuredProfileTests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/MeasuredProfileTests.py $<TARGET_FILE:OpenPuzzle>)
set_tests_properties(VanityMeasuredProfileTests PROPERTIES TIMEOUT 180)

add_test(NAME VanityResumeTests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ResumeTests.py $<TARGET_FILE:OpenPuzzle>)
set_tests_properties(VanityResumeTests PROPERTIES TIMEOUT 180)

# Native normal-client request path; strictly loopback-only in this stage.
target_sources(OpenPuzzleCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CoordinatorSession.cpp ${CMAKE_CURRENT_SOURCE_DIR}/VanityRunRoute.cpp)
target_compile_definitions(OpenPuzzleCore PRIVATE
    OPENPUZZLE_VANITY_COORDINATOR_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/measured/Coordinator.py")
add_executable(VanityCoordinatorPlanTests tests/CoordinatorPlanTests.cpp)
target_include_directories(VanityCoordinatorPlanTests PRIVATE ../../include ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(VanityCoordinatorPlanTests PRIVATE -UNDEBUG)
add_test(NAME VanityCoordinatorPlanTests COMMAND VanityCoordinatorPlanTests)
add_test(NAME VanityCoordinatorSessionTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/CoordinatorSessionTests.py
    $<TARGET_FILE:OpenPuzzle> $<TARGET_FILE:OpenPuzzleVanityAdapter>)
set_tests_properties(VanityCoordinatorSessionTests PROPERTIES TIMEOUT 180)

add_test(NAME VanityCoordinatorDesktopTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/CoordinatorDesktopTests.py $<TARGET_FILE:OpenPuzzle>)
set_tests_properties(VanityCoordinatorDesktopTests PROPERTIES TIMEOUT 180)
add_test(NAME VanityLocalCoordinatorTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/LocalCoordinatorTests.py)
set_tests_properties(VanityLocalCoordinatorTests PROPERTIES TIMEOUT 60)
add_test(NAME VanityCoordinatorRecoveryTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/CoordinatorRecoveryTests.py $<TARGET_FILE:OpenPuzzle>)
set_tests_properties(VanityCoordinatorRecoveryTests PROPERTIES TIMEOUT 180)
add_executable(VanityCoordinatorPacerTests tests/CoordinatorPacerTests.cpp)
target_include_directories(VanityCoordinatorPacerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(VanityCoordinatorPacerTests PRIVATE -UNDEBUG)
add_test(NAME VanityCoordinatorPacerTests COMMAND VanityCoordinatorPacerTests)
add_test(NAME VanityOwnerRecoveryTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/OwnerRecoveryTests.py $<TARGET_FILE:OpenPuzzle>)
set_tests_properties(VanityOwnerRecoveryTests PROPERTIES TIMEOUT 180)
add_executable(VanityRunSessionRouteTests tests/RunSessionRouteTests.cpp)
target_link_libraries(VanityRunSessionRouteTests PRIVATE OpenPuzzleCore)
target_compile_options(VanityRunSessionRouteTests PRIVATE -UNDEBUG)
add_test(NAME VanityRunSessionRouteTests COMMAND VanityRunSessionRouteTests)

# Undeployed receipt protocol proposal. No native build/GPU/network required.
add_test(NAME VanityReceiptLedgerTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/ReceiptLedgerTests.py)
add_test(NAME VanityReceiptRecoveryTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/ReceiptRecoveryTests.py)
add_test(NAME VanityEndpointPreflightTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/EndpointPreflightTests.py)
set_tests_properties(VanityReceiptLedgerTests VanityReceiptRecoveryTests VanityEndpointPreflightTests
    PROPERTIES TIMEOUT 60)

add_executable(VanityReceiptJsonTests tests/ReceiptJsonTests.cpp)
target_include_directories(VanityReceiptJsonTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(VanityReceiptJsonTests PRIVATE -UNDEBUG)
add_test(NAME VanityReceiptJsonTests COMMAND VanityReceiptJsonTests)
add_test(NAME VanityNativeReceiptTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/NativeReceiptTests.py
    $<TARGET_FILE:OpenPuzzle> $<TARGET_FILE:OpenPuzzleVanityAdapter>)
set_tests_properties(VanityNativeReceiptTests PROPERTIES TIMEOUT 180)

add_test(NAME VanityReceiptDesktopTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/ReceiptDesktopTests.py $<TARGET_FILE:OpenPuzzle>)
add_test(NAME VanityReceiptOwnerRecoveryTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/ReceiptOwnerRecoveryTests.py $<TARGET_FILE:OpenPuzzle>)
set_tests_properties(VanityReceiptDesktopTests VanityReceiptOwnerRecoveryTests PROPERTIES TIMEOUT 180)

add_executable(VanityCommonRuntimeDriver tests/CommonRuntimeDriver.cpp)
target_include_directories(VanityCommonRuntimeDriver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VanityCommonRuntimeDriver PRIVATE OpenPuzzleCore)
target_compile_options(VanityCommonRuntimeDriver PRIVATE -UNDEBUG)
add_test(NAME VanityCommonRuntimeTests COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/CommonRuntimeTests.py
    $<TARGET_FILE:VanityCommonRuntimeDriver> $<TARGET_FILE:OpenPuzzleVanityAdapter>)
set_tests_properties(VanityCommonRuntimeTests PROPERTIES TIMEOUT 180)
