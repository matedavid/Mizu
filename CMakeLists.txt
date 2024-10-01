cmake_minimum_required(VERSION 3.20)

project(Mizu)

# Global Project settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable FetchContent
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# MizuLib
add_subdirectory(Mizu/)

# Options
option(MIZU_BUILD_TESTS "Build Mizu Tests" ON)
option(MIZU_BUILD_EXAMPLES "Build Mizu Examples" ON)

if (MIZU_BUILD_TESTS)
    add_subdirectory(tests/)
endif ()

if (MIZU_BUILD_EXAMPLES)
    add_subdirectory(examples/)
endif ()

