project(Engine.Base LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_base_module.h"
    EXPORT_MACRO_NAME "MIZU_BASE_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_base_module)

set(MIZU_LOGGING_ENABLED ${MIZU_DEBUG})
target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_LOGGING_ENABLED=${MIZU_LOGGING_ENABLED})

set(MIZU_PROFILING_ENABLED ${MIZU_DEBUG})
target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_PROFILING_ENABLED=${MIZU_PROFILING_ENABLED})

#
# Dependencies
#

# glm
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_ENABLE_EXPERIMENTAL)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)

# magic_enum
FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG v0.9.8
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(magic_enum)

target_compile_definitions(${PROJECT_NAME} PUBLIC MAGIC_ENUM_RANGE_MIN=0)
target_compile_definitions(${PROJECT_NAME} PUBLIC MAGIC_ENUM_RANGE_MAX=64)

target_link_libraries(${PROJECT_NAME} PUBLIC magic_enum::magic_enum)

if (MIZU_LOGGING_ENABLED)

    # spdlog
    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.15.1
            GIT_PROGRESS TRUE
            GIT_SHALLOW TRUE
    )

    set(SPDLOG_BUILD_SHARED ON)

    FetchContent_MakeAvailable(spdlog)
    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)

endif ()

if (MIZU_PROFILING_ENABLED)

    # tracy
    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.13.1
            GIT_PROGRESS TRUE
            GIT_SHALLOW TRUE
    )

    set(TRACY_ENABLE    ON  CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON  CACHE BOOL "" FORCE)
    set(TRACY_STATIC    OFF CACHE BOOL "" FORCE)
    set(TRACY_FIBERS    ON  CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(tracy)
    target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)

    # add_subdirectory(${tracy_SOURCE_DIR}/profiler ${CMAKE_BINARY_DIR}/tracy-profiler-build)

endif ()

