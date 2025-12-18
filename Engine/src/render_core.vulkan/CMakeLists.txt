project(Engine.RenderCore.Vulkan LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_core_vulkan_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_VULKAN_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)

target_sources(${PROJECT_NAME}
    PRIVATE
        private/vulkan_acceleration_structure.cpp
        private/vulkan_buffer_resource.cpp
        private/vulkan_command_buffer.cpp
        private/vulkan_context.cpp
        private/vulkan_debug.cpp
        private/vulkan_descriptors.cpp
        private/vulkan_device.cpp
        private/vulkan_device_memory_allocator.cpp
        private/vulkan_framebuffer.cpp
        private/vulkan_image_resource.cpp
        private/vulkan_pipeline.cpp
        private/vulkan_pipeline_layout.cpp
        private/vulkan_queue.cpp
        private/vulkan_resource_group.cpp
        private/vulkan_resource_view.cpp
        private/vulkan_sampler_state.cpp
        private/vulkan_shader.cpp
        private/vulkan_swapchain.cpp
        private/vulkan_synchronization.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private
            FILES
				private/vulkan_acceleration_structure.h
				private/vulkan_buffer_resource.h
				private/vulkan_command_buffer.h
				private/vulkan_context.h
                private/vulkan_debug.h
				private/vulkan_descriptors.h
				private/vulkan_device.h
				private/vulkan_device_memory_allocator.h
				private/vulkan_framebuffer.h
				private/vulkan_image_resource.h
				private/vulkan_pipeline.h
				private/vulkan_pipeline_layout.h
				private/vulkan_queue.h
				private/vulkan_resource_group.h
				private/vulkan_resource_view.h
				private/vulkan_sampler_state.h
				private/vulkan_shader.h
				private/vulkan_swapchain.h
				private/vulkan_synchronization.h

    PUBLIC
        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_core_vulkan_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)

#
# Dependencies
#

# Vulkan
find_package(Vulkan)

if (Vulkan_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_RENDER_CORE_VULKAN_ENABLED=1)
endif ()

