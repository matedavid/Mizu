project(Engine.RenderCore LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_core_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)

target_sources(${PROJECT_NAME}
    PRIVATE
        private/rhi/buffer_resource.cpp
        private/rhi/command_buffer.cpp
        private/rhi/device.cpp
        private/rhi/image_resource.cpp
        private/rhi/resource_group.cpp
        private/rhi/rhi_helpers.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
				public/render_core/definitions/device_memory.h
				public/render_core/definitions/pipeline_layout.h
				public/render_core/definitions/rhi_window.h
                public/render_core/definitions/shader_types.h

                public/render_core/rhi/acceleration_structure.h
                public/render_core/rhi/buffer_resource.h
				public/render_core/rhi/command_buffer.h
                public/render_core/rhi/descriptors.h
				public/render_core/rhi/device_memory_allocator.h
				public/render_core/rhi/framebuffer.h
				public/render_core/rhi/image_resource.h
				public/render_core/rhi/pipeline.h
				public/render_core/rhi/resource_group.h
				public/render_core/rhi/resource_view.h
				public/render_core/rhi/rhi_helpers.h
				public/render_core/rhi/sampler_state.h
				public/render_core/rhi/shader.h
				public/render_core/rhi/swapchain.h
				public/render_core/rhi/synchronization.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_core_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_RENDER_CORE_VULKAN_DLL_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Engine.RenderCore.Vulkan.dll")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_RENDER_CORE_DX12_DLL_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Engine.RenderCore.Dx12.dll")

#
# Rhi
#

if (MIZU_PLATFORM_WINDOWS)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_RENDER_CORE_DX12_ENABLED=1)
endif()

find_package(Vulkan)
if (Vulkan_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_RENDER_CORE_VULKAN_ENABLED=1)
endif ()

