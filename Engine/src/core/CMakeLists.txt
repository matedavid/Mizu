project(Engine.Core LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_core_module.h"
    EXPORT_MACRO_NAME "MIZU_CORE_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_core_module)

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
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Asset)

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

