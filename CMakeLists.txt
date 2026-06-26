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

# The project is header-only except for main.cpp
add_executable(lob_sim main.cpp)

# Headers live in include/
target_include_directories(lob_sim PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Print build info
message(STATUS "Building LOB Simulation (C++17)")
message(STATUS "Output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lob_sim")
