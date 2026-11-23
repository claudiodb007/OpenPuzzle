cmake_minimum_required(VERSION 3.24)

# Keep the native engine a separate program from the MIT-licensed client.
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES "50-real;52-real;60-real;61-real;70-real;75-real;80-real;86-real;89-real;90-real;50-virtual")
endif()
project(OpenPuzzleVanity LANGUAGES CXX CUDA)

if(WIN32 AND (NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(FATAL_ERROR "Vanity for Windows requires the x64 MSVC toolchain and NVIDIA CUDA Toolkit")
endif()

add_executable(vanity-offline
    source/offline.cpp
    source/Base58.cpp
    source/Bech32.cpp
    source/GPU/GPUGenerate.cpp
    source/GPU/GPUEngine.cu
    source/Int.cpp
    source/IntGroup.cpp
    source/IntMod.cpp
    source/Point.cpp
    source/Random.cpp
    source/SECP256K1.cpp
    source/Timer.cpp
    source/Wildcard.cpp
    source/hash/ripemd160.cpp
    source/hash/ripemd160_sse.cpp
    source/hash/sha256.cpp
    source/hash/sha256_sse.cpp
    source/hash/sha512.cpp
)
set_target_properties(vanity-offline PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
    CUDA_RUNTIME_LIBRARY Static
)
find_package(CUDAToolkit REQUIRED)
target_include_directories(vanity-offline PRIVATE ${CUDAToolkit_INCLUDE_DIRS})

if(WIN32)
    target_include_directories(vanity-offline PRIVATE ../OpenPuzzle/include)
    target_compile_definitions(vanity-offline PRIVATE
        WIN32 WIN64 NOMINMAX _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601)
    target_compile_options(vanity-offline PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/FIcstdint>)
    target_link_options(vanity-offline PRIVATE "/STACK:8388608")
    target_link_libraries(vanity-offline PRIVATE advapi32)
    set_property(TARGET vanity-offline PROPERTY MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    find_package(Threads REQUIRED)
    target_compile_options(vanity-offline PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-include;cstdint;-mssse3;-msse4.1;-fno-strict-aliasing>"
        "$<$<COMPILE_LANGUAGE:CUDA>:-include;cstdint;-Xcompiler=-mssse3,-msse4.1>")
    target_link_libraries(vanity-offline PRIVATE Threads::Threads)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME VanityEngineIdentity COMMAND vanity-offline --identity)
    set_tests_properties(VanityEngineIdentity PROPERTIES
        PASS_REGULAR_EXPRESSION "^OpenPuzzle-VanityRange 0[.]2[.]0 CUDA target protocol 1")
    if(WIN32)
        add_executable(VanityWindowsNativeRunFilesTests
            tests/WindowsNativeRunFilesTests.cpp
            source/Base58.cpp
            source/hash/sha256.cpp
        )
        target_include_directories(VanityWindowsNativeRunFilesTests PRIVATE source ../OpenPuzzle/include)
        target_compile_features(VanityWindowsNativeRunFilesTests PRIVATE cxx_std_17)
        target_compile_definitions(VanityWindowsNativeRunFilesTests PRIVATE
            WIN64 NOMINMAX _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601)
        target_compile_options(VanityWindowsNativeRunFilesTests PRIVATE /FIcstdint)
        target_link_libraries(VanityWindowsNativeRunFilesTests PRIVATE advapi32)
        add_test(NAME VanityWindowsFiles COMMAND VanityWindowsNativeRunFilesTests --files)
        add_test(NAME VanityWindowsProcessOwnership COMMAND VanityWindowsNativeRunFilesTests --ownership)
    endif()
endif()
