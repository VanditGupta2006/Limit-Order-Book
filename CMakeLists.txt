# ==============================================================================
# CMakeLists.txt — LOB Simulation Build Configuration
# ==============================================================================
cmake_minimum_required(VERSION 3.16)
project(lob_sim LANGUAGES CXX)

# C++17 standard (required for std::filesystem, structured bindings)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Output binary to build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Headers live in include/
include_directories(${CMAKE_SOURCE_DIR}/include)

# ---- Main simulation executable ----
add_executable(lob_sim main.cpp)

# ---- GoogleTest (fetched at configure time) ----
# Disable TLS verification to prevent SSL certificate errors on some systems
set(CMAKE_TLS_VERIFY OFF)
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
# Prevent GoogleTest from overriding our compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ---- Unit test executable ----
add_executable(orderbook_test tests/test_orderbook.cpp)
target_link_libraries(orderbook_test GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(orderbook_test)

# ---- Google Benchmark (fetched at configure time) ----
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
)
FetchContent_MakeAvailable(googlebenchmark)

# ---- Benchmark executable ----
add_executable(orderbook_benchmark benchmarks/benchmark_orderbook.cpp)
target_link_libraries(orderbook_benchmark benchmark::benchmark benchmark::benchmark_main)

# Print build info
message(STATUS "Building LOB Simulation (C++17)")
message(STATUS "Output:     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lob_sim")
message(STATUS "Tests:      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/orderbook_test")
message(STATUS "Benchmarks: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/orderbook_benchmark")
