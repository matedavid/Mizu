project(Engine.RenderCore LANGUAGES CXX)

add_library(Engine.RenderCoreApi SHARED)

generate_export_header(Engine.RenderCoreApi
    EXPORT_FILE_NAME "mizu_render_core_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_API"
)

target_link_libraries(Engine.RenderCoreApi PRIVATE MizuProjectOptions)
set_target_properties(Engine.RenderCoreApi PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(Engine.RenderCoreApi
    PRIVATE
        private/definitions/resource.cpp
        private/rhi/acceleration_structure.cpp
        private/rhi/buffer_resource.cpp
        private/rhi/command_buffer.cpp
        private/rhi/device.cpp
        private/rhi/image_resource.cpp
        private/rhi/resource_view.cpp
        private/rhi/rhi_helpers.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
				public/render_core/definitions/device_memory.h
                public/render_core/definitions/resource.h
				public/render_core/definitions/rhi_window.h
                public/render_core/definitions/shader_types.h
                public/render_core/rhi/acceleration_structure.h
                public/render_core/rhi/buffer_resource.h
				public/render_core/rhi/command_buffer.h
                public/render_core/rhi/descriptors.h
				public/render_core/rhi/device_memory_allocator.h
				public/render_core/rhi/image_resource.h
				public/render_core/rhi/pipeline.h
                public/render_core/rhi/render_pass.h
				public/render_core/rhi/resource_view.h
				public/render_core/rhi/rhi_helpers.h
				public/render_core/rhi/sampler_state.h
				public/render_core/rhi/shader.h
				public/render_core/rhi/swapchain.h
				public/render_core/rhi/synchronization.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:Engine.RenderCoreApi,BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_core_module.h
)

target_link_libraries(Engine.RenderCoreApi PRIVATE Engine.Base)

target_compile_definitions(Engine.RenderCoreApi PRIVATE MIZU_RENDER_CORE_DX12_DLL_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Engine.RenderCore.Dx12.dll")
target_compile_definitions(Engine.RenderCoreApi PRIVATE MIZU_RENDER_CORE_VULKAN_DLL_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Engine.RenderCore.Vulkan.dll")

if (MIZU_BUILD_DX12 AND MIZU_PLATFORM_WINDOWS)
    target_compile_definitions(Engine.RenderCoreApi PUBLIC MIZU_RENDER_CORE_DX12_ENABLED=1)
    set(MIZU_RENDER_CORE_DX12_ENABLED 1)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../render_core.dx12 ${CMAKE_BINARY_DIR}/Engine/src/render_core.dx12)
endif()

if (MIZU_BUILD_VULKAN)
    find_package(Vulkan QUIET)
    if (Vulkan_FOUND)
        target_compile_definitions(Engine.RenderCoreApi PUBLIC MIZU_RENDER_CORE_VULKAN_ENABLED=1)
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
