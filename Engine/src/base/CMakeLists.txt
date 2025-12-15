project(Engine.Base LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_base_module.h"
    EXPORT_MACRO_NAME "MIZU_BASE_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)

target_sources(${PROJECT_NAME}
    PRIVATE
        private/io/filesystem.cpp
        private/math/bounding_box.cpp
        private/threads/fence.cpp
        private/threads/job_system.cpp
        private/types/uuid.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/base/containers/inplace_vector.h
                public/base/debug/assert.h
                public/base/debug/logging.h
                public/base/debug/profiling.h
                public/base/io/filesystem.h
                public/base/math/bounding_box.h
                public/base/threads/fence.h
                public/base/threads/job_system.h
                public/base/threads/ring_buffer.h
                public/base/types/uuid.h
                public/base/utils/enum_utils.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_base_module.h
)

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

if (MIZU_DEBUG)

    # spdlog
    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.15.1
            GIT_PROGRESS TRUE
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)

    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)

    # Tracy
    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.12.2
            GIT_PROGRESS TRUE
            GIT_SHALLOW TRUE
    )

    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)

    FetchContent_MakeAvailable(tracy)
    target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)

    # add_subdirectory(${tracy_SOURCE_DIR}/profiler ${CMAKE_BINARY_DIR}/tracy-profiler-build)

endif ()

