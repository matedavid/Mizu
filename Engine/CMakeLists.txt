project(MizuLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

if (MSVC)
    # /wd4715 removes the "not all paths return a value" warning
    # /wd4250 removes the "inherits x via dominance"
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /wd4715 /wd4250)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion -Werror)
endif ()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_PLATFORM_WINDOWS)
elseif (UNIX)
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_PLATFORM_UNIX)
else ()
    message(FATAL_ERROR "Platform not recognized")
endif ()

target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<CONFIG:DEBUG>:MIZU_DEBUG>)

target_sources(${PROJECT_NAME} PRIVATE
        # application
        src/application/application.cpp
        src/application/entry_point.cpp
        src/application/thread_sync.cpp
        src/application/window.cpp

        # base/io
        src/base/io/filesystem.cpp

        # base/math
        src/base/math/bounding_box.cpp

        # base/threads
        src/base/threads/fence.cpp

        # base/types
        src/base/types/uuid.cpp

        # input
        src/input/input.cpp

        # managers
        src/managers/shader_manager.cpp

        # renderer
        src/renderer/camera.cpp
        src/renderer/scene_renderer.cpp

        # renderer/deferred
        src/renderer/deferred/deferred_renderer.cpp

        # renderer/environment
        src/renderer/environment/environment.cpp

        # renderer/material
        src/renderer/material/material.cpp

        # renderer/model
        src/renderer/model/mesh.cpp

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

        # render_core/rhi/backend/opengl
        src/render_core/rhi/backend/opengl/opengl_backend.cpp
        src/render_core/rhi/backend/opengl/opengl_buffer_resource.cpp
        src/render_core/rhi/backend/opengl/opengl_command_buffer.cpp
        src/render_core/rhi/backend/opengl/opengl_compute_pipeline.cpp
        src/render_core/rhi/backend/opengl/opengl_context.cpp
        src/render_core/rhi/backend/opengl/opengl_device_memory_allocator.cpp
        src/render_core/rhi/backend/opengl/opengl_framebuffer.cpp
        src/render_core/rhi/backend/opengl/opengl_graphics_pipeline.cpp
        src/render_core/rhi/backend/opengl/opengl_image_resource.cpp
        src/render_core/rhi/backend/opengl/opengl_render_pass.cpp
        src/render_core/rhi/backend/opengl/opengl_resource_group.cpp
        src/render_core/rhi/backend/opengl/opengl_shader.cpp
        src/render_core/rhi/backend/opengl/opengl_synchronization.cpp

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
        src/state_manager/transform_state_manager.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_include_directories(${PROJECT_NAME} PUBLIC include/)

#
# Dependencies
#

# glm
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e # refs/tags/1.0.1
        GIT_PROGRESS TRUE
        SYSTEM
)
FetchContent_MakeAvailable(glm)

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_ENABLE_EXPERIMENTAL)

target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# spdlog
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.1
        GIT_PROGRESS TRUE
        SYSTEM
)
FetchContent_MakeAvailable(spdlog)

target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

# Spirv-cross
FetchContent_Declare(
        spirv-cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG vulkan-sdk-1.4.313.0
        GIT_PROGRESS TRUE
        SYSTEM
)
FetchContent_MakeAvailable(spirv-cross)

target_link_libraries(${PROJECT_NAME} PRIVATE spirv-cross-c)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})

# Glad
add_library(gladLib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/glad/src/glad.c)
target_include_directories(gladLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/glad/include/)

target_link_libraries(${PROJECT_NAME} PUBLIC gladLib)

# stb
add_library(stb STATIC)
target_sources(stb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb/stb_image.cpp)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb/)

target_link_libraries(${PROJECT_NAME} PRIVATE stb)

# entt
FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.2
        GIT_PROGRESS TRUE
        SYSTEM
)
FetchContent_MakeAvailable(entt)

target_link_libraries(${PROJECT_NAME} PUBLIC EnTT::EnTT)

#
# Extensions
#

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/extensions/)
target_link_libraries(${PROJECT_NAME} PUBLIC MizuExtensions)

#
# Shaders
#

include(cmake/shader_utils.cmake)

set(OUT_SHADER_PATH ${CMAKE_BINARY_DIR}/shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_PATH="${OUT_SHADER_PATH}")

file(MAKE_DIRECTORY ${OUT_SHADER_PATH})
set(ENGINE_SHADER_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

function(mizu_compile_engine_shaders)
    foreach (shader IN LISTS ARGN)
        get_filename_component(shader_name ${shader} NAME_WLE)
        get_filename_component(shader_ext ${shader} LAST_EXT)
        get_filename_component(folder_path ${shader} PATH)

        set(SHADER_PATH "${ENGINE_SHADER_BASE_PATH}/${shader}")

        get_filename_component(shader_comp_ext ${shader_name} LAST_EXT)
        if ("${shader_comp_ext}" STREQUAL ".comp")
            mizu_compile_slang_shader(${PROJECT_NAME} ${SHADER_PATH} "${OUT_SHADER_PATH}/${folder_path}/${shader_name}.spv" compute csMain ${ENGINE_SHADER_BASE_PATH})
        else ()
            mizu_compile_slang_shader(${PROJECT_NAME} ${SHADER_PATH} "${OUT_SHADER_PATH}/${folder_path}/${shader_name}.vert.spv" vertex vsMain ${ENGINE_SHADER_BASE_PATH})
            mizu_compile_slang_shader(${PROJECT_NAME} ${SHADER_PATH} "${OUT_SHADER_PATH}/${folder_path}/${shader_name}.frag.spv" fragment fsMain ${ENGINE_SHADER_BASE_PATH})
        endif ()

    endforeach ()
endfunction()

set(ENGINE_SHADERS
        deferred/Shadowmapping.slang
        deferred/PBROpaque.slang
        deferred/PBRLighting.slang
        deferred/Skybox.slang
        
        environment/IrradianceConvolution.slang
        environment/PrefilterEnvironment.slang

        presenter/Present.slang
)

mizu_compile_engine_shaders(${ENGINE_SHADERS})

mizu_compile_slang_shader(${PROJECT_NAME} ${ENGINE_SHADER_BASE_PATH}/deferred/SSAO.slang "${OUT_SHADER_PATH}/deferred/SSAOMain.comp.spv" compute ssaoMain ${ENGINE_SHADER_BASE_PATH})
mizu_compile_slang_shader(${PROJECT_NAME} ${ENGINE_SHADER_BASE_PATH}/deferred/SSAO.slang "${OUT_SHADER_PATH}/deferred/SSAOBlur.comp.spv" compute ssaoBlur ${ENGINE_SHADER_BASE_PATH})

mizu_compile_slang_shader(${PROJECT_NAME} ${ENGINE_SHADER_BASE_PATH}/environment/PrecomputeBRDF.slang "${OUT_SHADER_PATH}/environment/PrecomputeBRDF.comp.spv" compute precomputeBRDF ${ENGINE_SHADER_BASE_PATH})
