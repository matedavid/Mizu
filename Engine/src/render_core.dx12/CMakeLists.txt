project(Engine.RenderCore.Dx12 LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_render_core_dx12_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_DX12_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_render_core_dx12_module)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCoreApi)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_DX12_VALIDATIONS_ENABLED=${MIZU_DEBUG})
set(MIZU_DX12_VALIDATIONS_ENABLED ${MIZU_DEBUG})

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

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(arch "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(arch "arm64")
else()
    message(FATAL_ERROR "[MIZU]: Unsupported architecture for RenderCore.Dx12 dependencies: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(MIZU_DX12_VALIDATIONS_ENABLED)

    # WinPixEventRuntime

    set(MIZU_WINPIX_EVENT_RUNTIME_PACKAGE_VERSION 1.0.240308001)

    set(MIZU_WINPIX_EVENT_RUNTIME_PACKAGE_URL https://www.nuget.org/api/v2/package/WinPixEventRuntime/${MIZU_WINPIX_EVENT_RUNTIME_PACKAGE_VERSION})

    FetchContent_Declare(
        winpix_event_runtime
        URL ${MIZU_WINPIX_EVENT_RUNTIME_PACKAGE_URL}
    )

    FetchContent_MakeAvailable(winpix_event_runtime)

    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${winpix_event_runtime_SOURCE_DIR}/Include/WinPixEventRuntime/)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${winpix_event_runtime_SOURCE_DIR}/bin/${arch}/WinPixEventRuntime.lib)

    function(copy_winpix_event_runtime dll_name)
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${winpix_event_runtime_SOURCE_DIR}/bin/${arch}/${dll_name}
                    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${dll_name}
        )
    endfunction()

    copy_winpix_event_runtime(WinPixEventRuntime.dll)

endif()

# Agility SDK

set(MIZU_DX12_AGILITY_SDK_PACKAGE_VERSION 1.619.1)

string(REGEX MATCH "^[0-9]+\\.([0-9]+)\\." _ ${MIZU_DX12_AGILITY_SDK_PACKAGE_VERSION})
set(MIZU_DX12_AGILITY_SDK_VERSION ${CMAKE_MATCH_1})

set(MIZU_DX12_AGILITY_SDK_PACKAGE_URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${MIZU_DX12_AGILITY_SDK_PACKAGE_VERSION})

FetchContent_Declare(
    agility_sdk
    URL ${MIZU_DX12_AGILITY_SDK_PACKAGE_URL}
)

FetchContent_MakeAvailable(agility_sdk)

set(MIZU_DX12_AGILITY_SDK_PATH ${agility_sdk_SOURCE_DIR}/build/native/)

function(copy_agility_sdk dll_name)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MIZU_DX12_AGILITY_SDK_PATH}/bin/${arch}/${dll_name}
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/D3D12/${dll_name}
    )
endfunction()

copy_agility_sdk(D3D12Core.dll)
copy_agility_sdk(D3D12Core.pdb)
copy_agility_sdk(d3d12SDKLayers.dll)
copy_agility_sdk(d3d12SDKLayers.pdb)

target_include_directories(${PROJECT_NAME} PRIVATE ${MIZU_DX12_AGILITY_SDK_PATH}/include)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_RENDER_CORE_DX12_AGILITY_SDK_VERSION=${MIZU_DX12_AGILITY_SDK_VERSION})
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_RENDER_CORE_DX12_AGILITY_SDK_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/D3D12/")

