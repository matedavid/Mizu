project(MizuLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

if (MSVC)
    # /wd4715 removes the "not all paths return a value" warning
    # /wd4250 removes the "inherits x via dominance" warning
    target_compile_options(${PROJECT_NAME} PUBLIC /W4 /WX /wd4715 /wd4250)
else ()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wpedantic -Wextra -Wshadow -Wconversion -Werror)
endif ()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_PLATFORM_WINDOWS)
    set(MIZU_PLATFORM_WINDOWS 1)
elseif (UNIX)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_PLATFORM_UNIX)
    set(MIZU_PLATFORM_UNIX 1)
else ()
    message(FATAL_ERROR "Platform not recognized")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(MIZU_DEBUG 1)
endif ()

target_compile_definitions(${PROJECT_NAME} PUBLIC
        $<$<CONFIG:DEBUG>:MIZU_DEBUG>
        $<$<CONFIG:RELWITHDEBINFO>:MIZU_DEBUG>
)

set(MIZU_ENGINE_SHADERS_PATH ${CMAKE_BINARY_DIR}/shaders)
set(MIZU_ENGINE_SHADERS_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_PATH="${MIZU_ENGINE_SHADERS_PATH}")

target_sources(${PROJECT_NAME} PRIVATE
        # application
        src/application/application.cpp
        src/application/thread_sync.cpp
        src/application/window.cpp

        # base/io
        src/base/io/filesystem.cpp

        # base/math
        src/base/math/bounding_box.cpp

        # base/threads
        src/base/threads/fence.cpp
        src/base/threads/job_system.cpp

        # base/types
        src/base/types/uuid.cpp

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
        src/renderer/shader/shader_registry.cpp

        # render_core/render_graph
        src/render_core/render_graph/render_graph.cpp
        src/render_core/render_graph/render_graph_builder.cpp
        src/render_core/render_graph/render_graph_utils.cpp

        # render_core/resources
        src/render_core/resources/buffers.cpp
        src/render_core/resources/cubemap.cpp
        src/render_core/resources/texture.cpp

        # render_core/rhi
        src/render_core/rhi/buffer_resource.cpp
        src/render_core/rhi/command_buffer.cpp
        src/render_core/rhi/compute_pipeline.cpp
        src/render_core/rhi/device_memory_allocator.cpp
        src/render_core/rhi/framebuffer.cpp
        src/render_core/rhi/graphics_pipeline.cpp
        src/render_core/rhi/image_resource.cpp
        src/render_core/rhi/renderer.cpp
        src/render_core/rhi/render_pass.cpp
        src/render_core/rhi/resource_group.cpp
        src/render_core/rhi/resource_view.cpp
        src/render_core/rhi/rhi_helpers.cpp
        src/render_core/rhi/sampler_state.cpp
        src/render_core/rhi/shader.cpp
        src/render_core/rhi/swapchain.cpp
        src/render_core/rhi/synchronization.cpp

        # render_core/rhi/rtx
        src/render_core/rhi/rtx/acceleration_structure.cpp
        src/render_core/rhi/rtx/ray_tracing_pipeline.cpp

        # render_core/rhi/backend/directx12
        src/render_core/rhi/backend/directx12/dx12_backend.cpp
        src/render_core/rhi/backend/directx12/dx12_command_buffer.cpp
        src/render_core/rhi/backend/directx12/dx12_context.cpp
        src/render_core/rhi/backend/directx12/dx12_device.cpp

        # render_core/rhi/backend/vulkan
        src/render_core/rhi/backend/vulkan/vulkan_backend.cpp
        src/render_core/rhi/backend/vulkan/vulkan_buffer_resource.cpp
        src/render_core/rhi/backend/vulkan/vulkan_command_buffer.cpp
        src/render_core/rhi/backend/vulkan/vulkan_compute_pipeline.cpp
        src/render_core/rhi/backend/vulkan/vulkan_context.cpp
        src/render_core/rhi/backend/vulkan/vulkan_descriptors.cpp
        src/render_core/rhi/backend/vulkan/vulkan_device.cpp
        src/render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.cpp
        src/render_core/rhi/backend/vulkan/vulkan_framebuffer.cpp
        src/render_core/rhi/backend/vulkan/vulkan_graphics_pipeline.cpp
        src/render_core/rhi/backend/vulkan/vulkan_image_resource.cpp
        src/render_core/rhi/backend/vulkan/vulkan_instance.cpp
        src/render_core/rhi/backend/vulkan/vulkan_queue.cpp
        src/render_core/rhi/backend/vulkan/vulkan_render_pass.cpp
        src/render_core/rhi/backend/vulkan/vulkan_resource_group.cpp
        src/render_core/rhi/backend/vulkan/vulkan_resource_view.cpp
        src/render_core/rhi/backend/vulkan/vulkan_sampler_state.cpp
        src/render_core/rhi/backend/vulkan/vulkan_shader.cpp
        src/render_core/rhi/backend/vulkan/vulkan_synchronization.cpp
        src/render_core/rhi/backend/vulkan/vulkan_swapchain.cpp
        src/render_core/rhi/backend/vulkan/vulkan_utils.cpp

        # render_core/rhi/backend/vulkan/rtx
        src/render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.cpp
        src/render_core/rhi/backend/vulkan/rtx/vulkan_acceleration_structure.cpp
        src/render_core/rhi/backend/vulkan/rtx/vulkan_ray_tracing_pipeline.cpp

        # render_core/shader
        src/render_core/shader/shader_group.cpp
        src/render_core/shader/shader_reflection.cpp
        src/render_core/shader/shader_transpiler.cpp

        # render_core/systems
        src/render_core/systems/pipeline_cache.cpp
        src/render_core/systems/sampler_state_cache.cpp

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

add_library(MizuLibWithMain)
target_sources(MizuLibWithMain PRIVATE
        src/application/entry_point.cpp
        src/application/main_loop.cpp
)
target_link_libraries(MizuLibWithMain PUBLIC MizuLib)

#
# Dependencies
#

# glm
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        SYSTEM
)
FetchContent_MakeAvailable(glm)

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_ENABLE_EXPERIMENTAL)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)

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
        d3dcompiler
    )

endif()

# Spirv-cross
FetchContent_Declare(
        spirv-cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG vulkan-sdk-1.4.313.0
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        SYSTEM
)
FetchContent_MakeAvailable(spirv-cross)

target_link_libraries(${PROJECT_NAME} PRIVATE spirv-cross-c)

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
set(SLANG_VERSION 2025.18.2)

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


if (MIZU_DEBUG)

    # spdlog
    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.15.1
            GIT_PROGRESS TRUE
            GIT_SHALLOW TRUE
            SYSTEM
    )
    FetchContent_MakeAvailable(spdlog)

    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)

    # Tracy
    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.12.2
            GIT_PROGRESS TRUE
            GIT_SHALLOW TRUE
    )

    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)

    FetchContent_MakeAvailable(tracy)
    target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)

    add_subdirectory(${tracy_SOURCE_DIR}/profiler ${CMAKE_BINARY_DIR}/tracy-profiler-build)

endif ()

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

include(cmake/shader_utils.cmake)

file(MAKE_DIRECTORY ${MIZU_ENGINE_SHADERS_PATH})

function(mizu_compile_engine_shaders)
    foreach (shader IN LISTS ARGN)
        get_filename_component(shader_name ${shader} NAME_WLE)
        get_filename_component(shader_ext ${shader} LAST_EXT)
        get_filename_component(folder_path ${shader} PATH)

        set(SHADER_PATH "${MIZU_ENGINE_SHADERS_SOURCE_PATH}/${shader}")

        get_filename_component(shader_comp_ext ${shader_name} LAST_EXT)
        if ("${shader_comp_ext}" STREQUAL ".comp")
            mizu_compile_slang_shader(${PROJECT_NAME} ${SHADER_PATH} "${MIZU_ENGINE_SHADERS_PATH}/${folder_path}/${shader_name}.spv" compute csMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
        else ()
            mizu_compile_slang_shader(${PROJECT_NAME} ${SHADER_PATH} "${MIZU_ENGINE_SHADERS_PATH}/${folder_path}/${shader_name}.vert.spv" vertex vsMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
            mizu_compile_slang_shader(${PROJECT_NAME} ${SHADER_PATH} "${MIZU_ENGINE_SHADERS_PATH}/${folder_path}/${shader_name}.frag.spv" fragment fsMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
        endif ()

    endforeach ()
endfunction()

set(ENGINE_SHADERS
        deferred/Shadowmapping.slang
        # deferred/PBROpaque.slang
        deferred/PBRLighting.slang
        deferred/Skybox.slang

        environment/IrradianceConvolution.slang
        environment/PrefilterEnvironment.slang

        forwardplus/CascadedShadowMapping.slang
        forwardplus/DepthNormalsPrepass.slang
        forwardplus/LightCullingDebug.slang
        forwardplus/PBROpaque.slang

        presenter/Present.slang
)

mizu_compile_engine_shaders(${ENGINE_SHADERS})

mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/deferred/SSAO.slang "${MIZU_ENGINE_SHADERS_PATH}/deferred/SSAOMain.comp.spv" compute ssaoMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/deferred/SSAO.slang "${MIZU_ENGINE_SHADERS_PATH}/deferred/SSAOBlur.comp.spv" compute ssaoBlur ${MIZU_ENGINE_SHADERS_SOURCE_PATH})

mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/environment/PrecomputeBRDF.slang "${MIZU_ENGINE_SHADERS_PATH}/environment/PrecomputeBRDF.comp.spv" compute precomputeBRDF ${MIZU_ENGINE_SHADERS_SOURCE_PATH})

mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/forwardplus/CascadedShadowMappingDebug.slang "${MIZU_ENGINE_SHADERS_PATH}/forwardplus/CascadedShadowMappingDebug.vert.spv" vertex vsMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/forwardplus/CascadedShadowMappingDebug.slang "${MIZU_ENGINE_SHADERS_PATH}/forwardplus/CascadedShadowMappingDebugCascades.frag.spv" fragment fsCascadesMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/forwardplus/CascadedShadowMappingDebug.slang "${MIZU_ENGINE_SHADERS_PATH}/forwardplus/CascadedShadowMappingDebugTexture.frag.spv" fragment fsTextureMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})

mizu_compile_slang_shader(${PROJECT_NAME} ${MIZU_ENGINE_SHADERS_SOURCE_PATH}/forwardplus/LightCulling.slang "${MIZU_ENGINE_SHADERS_PATH}/forwardplus/LightCulling.comp.spv" compute csMain ${MIZU_ENGINE_SHADERS_SOURCE_PATH})
