project(Engine.Core LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_core_module.h"
    EXPORT_MACRO_NAME "MIZU_CORE_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/job_system/fibers.cpp
        private/job_system/fiber_stack_memory_pool.cpp
        private/job_system/job_system.cpp
        private/game_context.cpp
        private/input.cpp
        private/runtime.cpp
        private/window.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES
                public/core/job_system/fibers.h
                public/core/job_system/fiber_stack_memory_pool.h
                public/core/job_system/inplace_job_function.h
                public/core/job_system/intrusive_free_list.h
                public/core/job_system/job_system.h
                public/core/job_system/mpsc_queue.h
                public/core/job_system/work_stealing_deque.h
				public/core/events.h
				public/core/game_context.h
				public/core/input.h
                public/core/keycodes.h
                public/core/runtime.h
                public/core/window.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_core_module.h
)

# Assembly for fibers
if (MIZU_PLATFORM_WINDOWS)

    enable_language(ASM_MASM)
    target_sources(${PROJECT_NAME} PRIVATE private/job_system/fiber_switch_internal_windows.asm)

elseif (MIZU_PLATFORM_UNIX)

    enable_language(ASM)
    target_sources(${PROJECT_NAME} PRIVATE private/job_system/fiber_switch_internal_linux.s)

endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.StateManager)

#
# Dependencies
#

# glfw
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

