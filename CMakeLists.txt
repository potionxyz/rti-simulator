# ============================================================================
# RTI 3D Volumetric Simulator - Build Configuration
# ============================================================================
# CMake is the build system - it figures out how to compile everything
# on different platforms (Linux, macOS, Windows) without me having to
# write separate build scripts for each one.
#
# to build:
#   mkdir build && cd build
#   cmake ..
#   make
# ============================================================================

cmake_minimum_required(VERSION 3.16)

# project name, version, and language
project(rti-simulator
    VERSION 0.1.0
    DESCRIPTION "3D Radio Tomographic Imaging Simulator"
    LANGUAGES CXX
)

# using C++17 - need it for std::optional, structured bindings,
# and filesystem support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# don't fall back to compiler extensions - keep it portable
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Dependencies (all header-only, no install needed) ----

# Eigen - linear algebra library (matrices, vectors, solvers)
# this is the backbone of the reconstruction maths
include_directories(${CMAKE_SOURCE_DIR}/include)

# ---- Source files ----

# collect all .cpp files in src/
file(GLOB SOURCES "src/*.cpp")

# build the main executable
add_executable(rti-simulator ${SOURCES})

# include our header files
target_include_directories(rti-simulator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# compiler warnings - want to catch problems early
target_compile_options(rti-simulator PRIVATE
    -Wall       # all standard warnings
    -Wextra     # extra warnings beyond -Wall
    -Wpedantic  # strict ISO C++ compliance
)

# ---- Tests ----

# enable testing support
enable_testing()

# test executable (will add test files later)
# file(GLOB TEST_SOURCES "tests/*.cpp")
# add_executable(rti-tests ${TEST_SOURCES})
# target_include_directories(rti-tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
# add_test(NAME rti-tests COMMAND rti-tests)
