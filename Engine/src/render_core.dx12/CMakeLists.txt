project(Engine.RenderCore.Dx12 LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_core_dx12_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_DX12_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/dx12_buffer_resource.cpp
        private/dx12_command_buffer.cpp
        private/dx12_context.cpp
        private/dx12_debug.cpp
        private/dx12_descriptors.cpp
        private/dx12_descriptors2.cpp
        private/dx12_device.cpp
        private/dx12_device_memory_allocator.cpp
        private/dx12_framebuffer.cpp
        private/dx12_image_resource.cpp
        private/dx12_pipeline.cpp
        private/dx12_resource_group.cpp
        private/dx12_resource_view.cpp
        private/dx12_root_signature.cpp
        private/dx12_sampler_state.cpp
        private/dx12_shader.cpp
        private/dx12_swapchain.cpp
        private/dx12_synchronization.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private
            FILES
                private/dx12_buffer_resource.h
                private/dx12_command_buffer.h
                private/dx12_context.h
                private/dx12_debug.h
                private/dx12_descriptors.h
                private/dx12_descriptors2.h
                private/dx12_device.h
                private/dx12_device_memory_allocator.h
                private/dx12_framebuffer.h
                private/dx12_image_resource.h
                private/dx12_pipeline.h
                private/dx12_resource_group.h
                private/dx12_resource_view.h
                private/dx12_root_signature.h
                private/dx12_sampler_state.h
                private/dx12_shader.h
                private/dx12_swapchain.h
                private/dx12_synchronization.h

    PUBLIC
        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_core_dx12_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_DX12_VALIDATIONS_ENABLED=${MIZU_DEBUG})

#
# Dependencies
#

# D3D12
if (MIZU_PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        d3d12
        dxgi
        dxguid
        dxcore
    )
endif()

