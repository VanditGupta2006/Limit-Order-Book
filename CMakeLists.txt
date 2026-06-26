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

# Print build info
message(STATUS "Building LOB Simulation (C++17)")
message(STATUS "Output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lob_sim")
message(STATUS "Tests:  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/orderbook_test")
