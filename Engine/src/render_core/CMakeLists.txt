project(Engine.RenderCore LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_core_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)

target_sources(${PROJECT_NAME}
    PRIVATE
        private/render_graph/render_graph.cpp
        private/render_graph/render_graph_builder.cpp
        private/render_graph/render_graph_utils.cpp

        private/resources/buffers.cpp
        private/resources/cubemap.cpp
        private/resources/texture.cpp

        private/rhi/acceleration_structure.cpp
        private/rhi/buffer_resource.cpp
        private/rhi/command_buffer.cpp
        private/rhi/device_memory_allocator.cpp
        private/rhi/framebuffer.cpp
        private/rhi/image_resource.cpp
        private/rhi/pipeline.cpp
        private/rhi/renderer.cpp
        private/rhi/resource_group.cpp
        private/rhi/resource_view.cpp
        private/rhi/rhi_helpers.cpp
        private/rhi/sampler_state.cpp
        private/rhi/shader.cpp
        private/rhi/swapchain.cpp
        private/rhi/synchronization.cpp

        private/backend/dx12/dx12_backend.cpp
        private/backend/dx12/dx12_buffer_resource.cpp
        private/backend/dx12/dx12_command_buffer.cpp
        private/backend/dx12/dx12_context.cpp
        private/backend/dx12/dx12_descriptors.cpp
        private/backend/dx12/dx12_device.cpp
        private/backend/dx12/dx12_device_memory_allocator.cpp
        private/backend/dx12/dx12_framebuffer.cpp
        private/backend/dx12/dx12_image_resource.cpp
        private/backend/dx12/dx12_pipeline.cpp
        private/backend/dx12/dx12_resource_group.cpp
        private/backend/dx12/dx12_resource_view.cpp
        private/backend/dx12/dx12_root_signature.cpp
        private/backend/dx12/dx12_sampler_state.cpp
        private/backend/dx12/dx12_shader.cpp
        private/backend/dx12/dx12_swapchain.cpp
        private/backend/dx12/dx12_synchronization.cpp

        private/backend/vulkan/vulkan_acceleration_structure.cpp
        private/backend/vulkan/vulkan_backend.cpp
        private/backend/vulkan/vulkan_buffer_resource.cpp
        private/backend/vulkan/vulkan_command_buffer.cpp
        private/backend/vulkan/vulkan_context.cpp
        private/backend/vulkan/vulkan_descriptors.cpp
        private/backend/vulkan/vulkan_device.cpp
        private/backend/vulkan/vulkan_device_memory_allocator.cpp
        private/backend/vulkan/vulkan_framebuffer.cpp
        private/backend/vulkan/vulkan_image_resource.cpp
        private/backend/vulkan/vulkan_instance.cpp
        private/backend/vulkan/vulkan_pipeline.cpp
        private/backend/vulkan/vulkan_pipeline_layout.cpp
        private/backend/vulkan/vulkan_queue.cpp
        private/backend/vulkan/vulkan_resource_group.cpp
        private/backend/vulkan/vulkan_resource_view.cpp
        private/backend/vulkan/vulkan_sampler_state.cpp
        private/backend/vulkan/vulkan_shader.cpp
        private/backend/vulkan/vulkan_swapchain.cpp
        private/backend/vulkan/vulkan_synchronization.cpp
        private/backend/vulkan/vulkan_utils.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private
            FILES
                private/render_graph/render_graph_resource_aliasing.h

                private/backend/dx12/dx12_backend.h
                private/backend/dx12/dx12_buffer_resource.h
                private/backend/dx12/dx12_command_buffer.h
                private/backend/dx12/dx12_context.h
                private/backend/dx12/dx12_descriptors.h
                private/backend/dx12/dx12_device.h
                private/backend/dx12/dx12_device_memory_allocator.h
                private/backend/dx12/dx12_framebuffer.h
                private/backend/dx12/dx12_image_resource.h
                private/backend/dx12/dx12_pipeline.h
                private/backend/dx12/dx12_resource_group.h
                private/backend/dx12/dx12_resource_view.h
                private/backend/dx12/dx12_root_signature.h
                private/backend/dx12/dx12_sampler_state.h
                private/backend/dx12/dx12_shader.h
                private/backend/dx12/dx12_swapchain.h
                private/backend/dx12/dx12_synchronization.h

				private/backend/vulkan/vulkan_acceleration_structure.h
				private/backend/vulkan/vulkan_backend.h
				private/backend/vulkan/vulkan_buffer_resource.h
				private/backend/vulkan/vulkan_command_buffer.h
				private/backend/vulkan/vulkan_context.h
				private/backend/vulkan/vulkan_descriptors.h
				private/backend/vulkan/vulkan_device.h
				private/backend/vulkan/vulkan_device_memory_allocator.h
				private/backend/vulkan/vulkan_framebuffer.h
				private/backend/vulkan/vulkan_image_resource.h
				private/backend/vulkan/vulkan_instance.h
				private/backend/vulkan/vulkan_pipeline.h
				private/backend/vulkan/vulkan_pipeline_layout.h
				private/backend/vulkan/vulkan_queue.h
				private/backend/vulkan/vulkan_resource_group.h
				private/backend/vulkan/vulkan_resource_view.h
				private/backend/vulkan/vulkan_sampler_state.h
				private/backend/vulkan/vulkan_shader.h
				private/backend/vulkan/vulkan_swapchain.h
				private/backend/vulkan/vulkan_synchronization.h
				private/backend/vulkan/vulkan_utils.h

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
				public/render_core/definitions/device_memory.h
				public/render_core/definitions/pipeline_layout.h
				public/render_core/definitions/rhi_window.h
                public/render_core/definitions/shader_types.h

                public/render_core/render_graph/render_graph.h
                public/render_core/render_graph/render_graph_blackboard.h
                public/render_core/render_graph/render_graph_builder.h
                public/render_core/render_graph/render_graph_resources.h
                public/render_core/render_graph/render_graph_shader_parameters.h
                public/render_core/render_graph/render_graph_types.h
                public/render_core/render_graph/render_graph_utils.h

                public/render_core/resources/buffers.h
                public/render_core/resources/cubemap.h
                public/render_core/resources/texture.h

                public/render_core/rhi/acceleration_structure.h
                public/render_core/rhi/buffer_resource.h
				public/render_core/rhi/command_buffer.h
				public/render_core/rhi/device_memory_allocator.h
				public/render_core/rhi/framebuffer.h
				public/render_core/rhi/image_resource.h
				public/render_core/rhi/pipeline.h
				public/render_core/rhi/renderer.h
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

#
# Dependencies
#

# Vulkan
find_package(Vulkan)

if (Vulkan_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_RENDER_CORE_VULKAN_ENABLED=1)
endif ()

# D3D12
if (MIZU_PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        d3d12
        dxgi
        dxguid
        dxcore
    )

    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_RENDER_CORE_DX12_ENABLED=1)
endif()

# stb
add_library(mizu_stb STATIC)
target_sources(mizu_stb PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image.cpp)
target_include_directories(mizu_stb PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb/)

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)
