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
        private/dx12_types.cpp

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
                private/dx12_types.h

    PUBLIC
        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_core_dx12_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCoreApi)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_DX12_VALIDATIONS_ENABLED=${MIZU_DEBUG})

#
# Dependencies
#

# D3D12

target_link_libraries(${PROJECT_NAME} PUBLIC
    d3d12
    dxgi
    dxguid
    dxcore
)

# Agility SDK

set(MIZU_DX12_AGILITY_SDK_PACKAGE_VERSION 1.619.1)

string(REGEX MATCH "^[0-9]+\\.([0-9]+)\\." _ ${MIZU_DX12_AGILITY_SDK_PACKAGE_VERSION})
set(MIZU_DX12_AGILITY_SDK_VERSION ${CMAKE_MATCH_1})

set(MIZU_DX12_AGILITY_SDK_PACKAGE_URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${MIZU_DX12_AGILITY_SDK_PACKAGE_VERSION})
message(STATUS ${MIZU_DX12_AGILITY_SDK_PACKAGE_URL})

FetchContent_Declare(
    agility_sdk
    URL ${MIZU_DX12_AGILITY_SDK_PACKAGE_URL}
)

FetchContent_MakeAvailable(agility_sdk)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(arch "x64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(arch "arm64")
else ()
    message(FATAL_ERROR "[MIZU]: Unsupported architecture for agility sdk: ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

set(MIZU_DX12_AGILITY_SDK_PATH ${agility_sdk_SOURCE_DIR}/build/native/)

function (copy_agility_sdk dll_name)
    add_custom_command(
        TARGET  ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MIZU_DX12_AGILITY_SDK_PATH}/bin/${arch}/${dll_name}
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/D3D12/${dll_name}
    )
endfunction ()

copy_agility_sdk(D3D12Core.dll)
copy_agility_sdk(D3D12Core.pdb)
copy_agility_sdk(d3d12SDKLayers.dll)
copy_agility_sdk(d3d12SDKLayers.pdb)

target_include_directories(${PROJECT_NAME} PRIVATE ${MIZU_DX12_AGILITY_SDK_PATH}/include)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_RENDER_CORE_DX12_AGILITY_SDK_VERSION=${MIZU_DX12_AGILITY_SDK_VERSION})
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_RENDER_CORE_DX12_AGILITY_SDK_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/D3D12/")

