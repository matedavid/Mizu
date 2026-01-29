cmake_minimum_required(VERSION 3.23)

project(Mizu)

# Global Project Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Cmake features
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

include(GenerateExportHeader)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(MIZU_USE_UNITY_BUILD ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Engine)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Engine)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Engine)

# MizuLib
add_subdirectory(Engine/)

# Options
option(MIZU_BUILD_TESTS "Build Mizu Tests" ON)
option(MIZU_BUILD_EXAMPLES "Build Mizu Examples" ON)

if (MIZU_BUILD_TESTS)
    add_subdirectory(tests/)
endif ()

if (MIZU_BUILD_EXAMPLES)
    add_subdirectory(examples/)
endif ()

