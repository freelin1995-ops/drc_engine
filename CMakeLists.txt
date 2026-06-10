cmake_minimum_required(VERSION 3.16)
project(drc-engine VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(DRC_BUILD_CLI "Build CLI tool" ON)
option(DRC_BUILD_TESTS "Build tests" ON)

find_package(Lua REQUIRED)
find_package(ZLIB REQUIRED)
option(DRC_USE_MPI "Build with MPI distributed support" OFF)

if(DRC_USE_MPI)
    find_package(MPI REQUIRED COMPONENTS CXX)
endif()

add_library(sol INTERFACE)
target_include_directories(sol INTERFACE ${CMAKE_SOURCE_DIR}/3rd/sol)

add_subdirectory(src/tl)
add_subdirectory(src/db)
add_subdirectory(src/gds)
add_subdirectory(src/rdb)
add_subdirectory(src/drc)

if(DRC_BUILD_CLI)
    add_subdirectory(src/cli)
endif()
if(DRC_USE_MPI)
    add_subdirectory(src/mpi)
endif()

if(DRC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
