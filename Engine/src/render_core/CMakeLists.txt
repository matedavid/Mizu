project(Engine.RenderCore LANGUAGES CXX)

add_library(Engine.RenderCoreApi SHARED)

generate_export_header(Engine.RenderCoreApi
    EXPORT_FILE_NAME "mizu_render_core_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_API"
)

mizu_configure_module(Engine.RenderCoreApi)
mizu_set_module_sources(Engine.RenderCoreApi mizu_render_core_module)

target_link_libraries(Engine.RenderCoreApi PRIVATE Engine.Base)

if (MIZU_BUILD_DX12 AND MIZU_PLATFORM_WINDOWS)
    target_compile_definitions(Engine.RenderCoreApi PUBLIC MIZU_RENDER_CORE_DX12_ENABLED=1)
    target_compile_definitions(Engine.RenderCoreApi PRIVATE MIZU_RENDER_CORE_DX12_DLL_PATH="$<TARGET_FILE:Engine.RenderCore.Dx12>")
    set(MIZU_RENDER_CORE_DX12_ENABLED 1)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../render_core.dx12 ${CMAKE_BINARY_DIR}/Engine/src/render_core.dx12)
endif()

if (MIZU_BUILD_VULKAN)
    find_package(Vulkan QUIET)
    if (Vulkan_FOUND)
        target_compile_definitions(Engine.RenderCoreApi PUBLIC MIZU_RENDER_CORE_VULKAN_ENABLED=1)
        target_compile_definitions(Engine.RenderCoreApi PRIVATE MIZU_RENDER_CORE_VULKAN_DLL_PATH="$<TARGET_FILE:Engine.RenderCore.Vulkan>")
        set(MIZU_RENDER_CORE_VULKAN_ENABLED 1)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../render_core.vulkan ${CMAKE_BINARY_DIR}/Engine/src/render_core.vulkan)
    endif()
endif()

add_library(Engine.RenderCore INTERFACE)
target_link_libraries(Engine.RenderCore INTERFACE 
    Engine.RenderCoreApi
    $<$<BOOL:${MIZU_RENDER_CORE_DX12_ENABLED}>:Engine.RenderCore.Dx12>
    $<$<BOOL:${MIZU_RENDER_CORE_VULKAN_ENABLED}>:Engine.RenderCore.Vulkan>
)
