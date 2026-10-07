# Isolate ported process and storage components from the remaining runtime,
# network, GPU and UI dependencies of the full application.
if(WIN32)
    add_library(OpenPuzzlePlatform
        ../src/core/WindowsProcessRunner.cpp
        ../src/runtime/WindowsProcessIdentity.cpp
        ../src/core/ProcessRunnerFactory.cpp)
    target_compile_definitions(OpenPuzzlePlatform PRIVATE _WIN32_WINNT=0x0601)
    add_executable(WindowsProcessFixture ../tests/windows/WindowsProcessFixture.cpp)
    add_executable(WindowsProcessRunnerTests ../tests/windows/WindowsProcessRunnerTests.cpp)
    target_link_libraries(WindowsProcessRunnerTests PRIVATE OpenPuzzlePlatform)
    add_test(NAME WindowsProcessRunnerTests COMMAND WindowsProcessRunnerTests $<TARGET_FILE:WindowsProcessFixture>)
    add_executable(WindowsProcessIdentityTests ../tests/windows/WindowsProcessIdentityTests.cpp)
    target_link_libraries(WindowsProcessIdentityTests PRIVATE OpenPuzzlePlatform)
    add_test(NAME WindowsProcessIdentityTests COMMAND WindowsProcessIdentityTests $<TARGET_FILE:WindowsProcessFixture>)
    if(MINGW)
        foreach(name WindowsProcessFixture WindowsProcessRunnerTests WindowsProcessIdentityTests)
            target_link_options(${name} PRIVATE -municode)
        endforeach()
    endif()
    set_tests_properties(WindowsProcessRunnerTests WindowsProcessIdentityTests PROPERTIES TIMEOUT 40)
else()
    add_library(OpenPuzzlePlatform ../src/core/PosixProcessRunner.cpp ../src/core/ProcessRunnerFactory.cpp)
    foreach(name PosixProcessRunnerTests PosixProcessRunnerTimeoutTests)
        add_executable(${name} ../tests/core/${name}.cpp)
        target_link_libraries(${name} PRIVATE OpenPuzzlePlatform)
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES TIMEOUT 40)
    endforeach()
endif()
target_include_directories(OpenPuzzlePlatform PUBLIC ../include)

# This stage also compiles the storage components; no runtime or API is started.
add_library(OpenPuzzleStorage
    ../src/config/ConfigurationManager.cpp
    ../src/client/ClientIdentity.cpp
    ../src/client/ClientStateStore.cpp
    ../src/runtime/WorkspaceSecurity.cpp
    ../src/runtime/ExecutionPersistence.cpp
    ../src/runtime/ExecutionSlot.cpp
    ../src/core/RecoveryManager.cpp
    ../src/core/WorkspaceManager.cpp)
target_include_directories(OpenPuzzleStorage PUBLIC ../include)
set(OPENPUZZLE_PLATFORM_BOOST_INCLUDE_DIR "" CACHE PATH "Boost headers for isolated storage compilation")
if(OPENPUZZLE_PLATFORM_BOOST_INCLUDE_DIR)
    target_include_directories(OpenPuzzleStorage PRIVATE "${OPENPUZZLE_PLATFORM_BOOST_INCLUDE_DIR}")
endif()
if(WIN32)
    target_link_libraries(OpenPuzzleStorage PUBLIC advapi32)
endif()
if(MSVC)
    target_compile_options(OpenPuzzleStorage PRIVATE /utf-8)
endif()
add_executable(PortableStorageTests ../tests/core/PortableStorageTests.cpp)
target_link_libraries(PortableStorageTests PRIVATE OpenPuzzleStorage)
if(MSVC)
    target_compile_options(PortableStorageTests PRIVATE /utf-8)
endif()
add_test(NAME PortableStorageTests COMMAND PortableStorageTests)
set_tests_properties(PortableStorageTests PROPERTIES TIMEOUT 45)

if(WIN32)
    add_executable(WindowsStorageIdentityTests ../tests/windows/WindowsStorageIdentityTests.cpp)
    target_link_libraries(WindowsStorageIdentityTests PRIVATE OpenPuzzleStorage)
    if(MSVC)
        target_compile_options(WindowsStorageIdentityTests PRIVATE /utf-8)
    elseif(MINGW)
        target_link_options(WindowsStorageIdentityTests PRIVATE -municode)
    endif()
    add_test(NAME WindowsStorageIdentityTests COMMAND WindowsStorageIdentityTests)
    set_tests_properties(WindowsStorageIdentityTests PROPERTIES TIMEOUT 45)
endif()
