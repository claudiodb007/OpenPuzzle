if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Vanity preview currently requires Linux")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
target_sources(OpenPuzzleCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/VanityCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/VanityPreviewEngine.cpp
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
target_sources(OpenPuzzleCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CoordinatorSession.cpp)
add_executable(VanityCoordinatorPlanTests tests/CoordinatorPlanTests.cpp)
target_include_directories(VanityCoordinatorPlanTests PRIVATE ../../include ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(VanityCoordinatorPlanTests PRIVATE -UNDEBUG)
add_test(NAME VanityCoordinatorPlanTests COMMAND VanityCoordinatorPlanTests)
add_test(NAME VanityCoordinatorSessionTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/CoordinatorSessionTests.py
    $<TARGET_FILE:OpenPuzzle> $<TARGET_FILE:OpenPuzzleVanityAdapter>)
set_tests_properties(VanityCoordinatorSessionTests PROPERTIES TIMEOUT 180)
