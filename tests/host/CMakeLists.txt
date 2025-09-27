cmake_minimum_required(VERSION 3.20)
project(SensoryBridgeHostTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

set(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

add_library(sb_core INTERFACE)
target_include_directories(sb_core INTERFACE
    ${PROJECT_ROOT}/src
    ${PROJECT_ROOT}/include
    ${PROJECT_ROOT}/tests/host/stubs
    ${PROJECT_ROOT}/libraries/FixedPoints/src
)

enable_testing()

add_executable(host_tests
    audio_processed_state_tests.cpp
)

target_link_libraries(host_tests PRIVATE sb_core GTest::gtest_main)

gtest_discover_tests(host_tests)
