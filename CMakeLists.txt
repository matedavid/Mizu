cmake_minimum_required(VERSION 3.20)

project(Mizu)

# Global Project settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

# Enable FetchContent
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# MizuLib
add_subdirectory(src/)
target_include_directories(MizuLib PUBLIC include/)

# Options
option(MIZU_BUILD_TESTS "Build Mizu Tests" ON)
option(MIZU_BUILD_APPS "Build Mizu Apps" ON)

if (MIZU_BUILD_TESTS)
    add_subdirectory(tests/)
endif ()

if (MIZU_BUILD_APPS)
    add_subdirectory(apps/)
endif ()

