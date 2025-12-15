project(MizuEngineLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

include(cmake/CommonConfig.cmake)

set(MIZU_ENGINE_SHADERS_PATH ${CMAKE_BINARY_DIR}/shaders)
set(MIZU_ENGINE_SHADERS_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_PATH="${MIZU_ENGINE_SHADERS_PATH}")

target_sources(${PROJECT_NAME} PRIVATE
        # application
        src/application/application.cpp
        src/application/thread_sync.cpp
        src/application/window.cpp

        # input
        src/input/input.cpp

        # renderer
        src/renderer/camera.cpp
        src/renderer/draw_block_manager.cpp
        src/renderer/render_graph_renderer.cpp
        src/renderer/scene_renderer.cpp

        # renderer/environment
        src/renderer/environment/environment.cpp

        # renderer/material
        src/renderer/material/material.cpp

        # renderer/model
        src/renderer/model/mesh.cpp

        # renderer/shader
        src/renderer/shader/shader_compiler.cpp
        src/renderer/shader/shader_manager.cpp
        src/renderer/shader/shader_reflection.cpp
        src/renderer/shader/shader_registry.cpp

        # renderer/systems
        src/renderer/systems/pipeline_cache.cpp
        src/renderer/systems/sampler_state_cache.cpp

        # render_core/render_graph
        src/render_core/render_graph/render_graph.cpp
        src/render_core/render_graph/render_graph_builder.cpp
        src/render_core/render_graph/render_graph_utils.cpp

        # render_core/resources
        src/render_core/resources/buffers.cpp
        src/render_core/resources/cubemap.cpp
        src/render_core/resources/texture.cpp

        # render_core/rhi
        src/render_core/rhi/acceleration_structure.cpp
        src/render_core/rhi/buffer_resource.cpp
        src/render_core/rhi/command_buffer.cpp
        src/render_core/rhi/device_memory_allocator.cpp
        src/render_core/rhi/framebuffer.cpp
        src/render_core/rhi/image_resource.cpp
        src/render_core/rhi/pipeline.cpp
        src/render_core/rhi/renderer.cpp
        src/render_core/rhi/resource_group.cpp
        src/render_core/rhi/resource_view.cpp
        src/render_core/rhi/rhi_helpers.cpp
        src/render_core/rhi/sampler_state.cpp
        src/render_core/rhi/shader.cpp
        src/render_core/rhi/swapchain.cpp
        src/render_core/rhi/synchronization.cpp

        # render_core/rhi/backend/directx12
        src/render_core/rhi/backend/directx12/dx12_backend.cpp
        src/render_core/rhi/backend/directx12/dx12_buffer_resource.cpp
        src/render_core/rhi/backend/directx12/dx12_command_buffer.cpp
        src/render_core/rhi/backend/directx12/dx12_context.cpp
        src/render_core/rhi/backend/directx12/dx12_descriptors.cpp
        src/render_core/rhi/backend/directx12/dx12_device.cpp
        src/render_core/rhi/backend/directx12/dx12_device_memory_allocator.cpp
        src/render_core/rhi/backend/directx12/dx12_framebuffer.cpp
        src/render_core/rhi/backend/directx12/dx12_image_resource.cpp
        src/render_core/rhi/backend/directx12/dx12_pipeline.cpp
        src/render_core/rhi/backend/directx12/dx12_resource_group.cpp
        src/render_core/rhi/backend/directx12/dx12_resource_view.cpp
        src/render_core/rhi/backend/directx12/dx12_root_signature.cpp
        src/render_core/rhi/backend/directx12/dx12_sampler_state.cpp
        src/render_core/rhi/backend/directx12/dx12_shader.cpp
        src/render_core/rhi/backend/directx12/dx12_swapchain.cpp
        src/render_core/rhi/backend/directx12/dx12_synchronization.cpp

        # render_core/rhi/backend/vulkan
        src/render_core/rhi/backend/vulkan/vulkan_acceleration_structure.cpp
        src/render_core/rhi/backend/vulkan/vulkan_backend.cpp
        src/render_core/rhi/backend/vulkan/vulkan_buffer_resource.cpp
        src/render_core/rhi/backend/vulkan/vulkan_command_buffer.cpp
        src/render_core/rhi/backend/vulkan/vulkan_context.cpp
        src/render_core/rhi/backend/vulkan/vulkan_descriptors.cpp
        src/render_core/rhi/backend/vulkan/vulkan_device.cpp
        src/render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.cpp
        src/render_core/rhi/backend/vulkan/vulkan_framebuffer.cpp
        src/render_core/rhi/backend/vulkan/vulkan_image_resource.cpp
        src/render_core/rhi/backend/vulkan/vulkan_instance.cpp
        src/render_core/rhi/backend/vulkan/vulkan_pipeline.cpp
        src/render_core/rhi/backend/vulkan/vulkan_pipeline_layout.cpp
        src/render_core/rhi/backend/vulkan/vulkan_queue.cpp
        src/render_core/rhi/backend/vulkan/vulkan_resource_group.cpp
        src/render_core/rhi/backend/vulkan/vulkan_resource_view.cpp
        src/render_core/rhi/backend/vulkan/vulkan_sampler_state.cpp
        src/render_core/rhi/backend/vulkan/vulkan_shader.cpp
        src/render_core/rhi/backend/vulkan/vulkan_synchronization.cpp
        src/render_core/rhi/backend/vulkan/vulkan_swapchain.cpp
        src/render_core/rhi/backend/vulkan/vulkan_utils.cpp

        # render_core/shader
        src/render_core/shader/shader_group.cpp

        # scene
        src/scene/scene.cpp

        # state_manager
        src/state_manager/base_state_manager.inl.cpp
        src/state_manager/camera_state_manager.cpp
        src/state_manager/imgui_state_manager.cpp
        src/state_manager/light_state_manager.cpp
        src/state_manager/renderer_settings_state_manager.cpp
        src/state_manager/state_manager_coordinator.cpp
        src/state_manager/transform_state_manager.cpp
        src/state_manager/static_mesh_state_manager.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PUBLIC MizuProjectOptions)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/base/)
target_link_libraries(${PROJECT_NAME} PUBLIC Engine.Base)

add_library(MizuEngine)
target_link_libraries(MizuEngine PUBLIC ${PROJECT_NAME})

add_dependencies(MizuEngine MizuShaderPipeline)

add_library(MizuEngineWithMain)
target_sources(MizuEngineWithMain PRIVATE
        src/application/entry_point.cpp
        src/application/main_loop.cpp
)
target_link_libraries(MizuEngineWithMain PUBLIC MizuEngine)

#
# Extensions
#

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/extensions/)
target_link_libraries(${PROJECT_NAME} PUBLIC MizuExtensions)

#
# Tools
#

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools/)

#
# Shaders
#

mizu_shader_pipeline_add_shader_provider_define(src/renderer/render_graph_renderer_shaders_provider.cpp MIZU_ENGINE_SHADERS_SOURCE_PATH="${MIZU_ENGINE_SHADERS_SOURCE_PATH}")
mizu_shader_pipeline_add_shader_provider(src/renderer/material/material_shader_provider.cpp)
mizu_shader_pipeline_add_shader_provider(src/renderer/environment/environment_shader_provider.cpp)

#
# Dependencies
#

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})


if (MIZU_PLATFORM_WINDOWS)

    # DirectX12
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d12
        dxgi
        dxguid
        dxcore
    )

endif()

# stb
add_library(mizu_stb STATIC)
target_sources(mizu_stb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb/stb_image.cpp)
target_include_directories(mizu_stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb/)

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

# entt
FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.2
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        SYSTEM
)
FetchContent_MakeAvailable(entt)

target_link_libraries(${PROJECT_NAME} PUBLIC EnTT::EnTT)

# slang
set(SLANG_VERSION 2025.19.1)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(arch "x86_64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(arch "aarch64")
else ()
    message(FATAL_ERROR "[MIZU]: Unsupported architecture for slang binary releases: ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(os "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(os "macos")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(os "linux")
else ()
    message(FATAL_ERROR "[MIZU]: Unsupported operating system for slang binary releases: ${CMAKE_SYSTEM_NAME}")
endif ()

set(slang_url "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${os}-${arch}.zip")

FetchContent_Declare(slang URL ${slang_url})
FetchContent_MakeAvailable(slang)
message(STATUS "[MIZU]: Finished downloading slang: ${slang_SOURCE_DIR}")

add_library(mizu_slang SHARED IMPORTED GLOBAL)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (MSVC)
		set_target_properties(mizu_slang
			PROPERTIES
				IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang.lib"
				IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang.dll"
		)
	else()
		message(FATAL_ERROR "Sorry, Slang does not provide precompiled binaries for MSYS/MinGW")
	endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set_target_properties(mizu_slang
		PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/libslang.so"
			IMPORTED_NO_SONAME TRUE
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set_target_properties(mizu_slang
		PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/libslang.dylib"
	)
endif()

target_include_directories(mizu_slang INTERFACE "${slang_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PUBLIC mizu_slang)

# json 
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)

