project(MizuLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion)
endif ()

target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<CONFIG:DEBUG>:MIZU_DEBUG>)

target_sources(${PROJECT_NAME} PRIVATE
        # core
        src/core/application.cpp
        src/core/bounding_box.cpp
        src/core/uuid.cpp
        src/core/window.cpp

        # input
        src/input/input.cpp

        # managers
        src/managers/shader_manager.cpp

        # renderer/deferred
        src/renderer/deferred/deferred_renderer.cpp

        # render_core/render_graph
        src/render_core/render_graph/render_graph.cpp
        src/render_core/render_graph/render_graph_builder.cpp
        src/render_core/render_graph/render_graph_dependencies.cpp

        # render_core/resources
        src/render_core/resources/buffers.cpp
        src/render_core/resources/camera.cpp
        src/render_core/resources/cubemap.cpp
        src/render_core/resources/material.cpp
        src/render_core/resources/mesh.cpp
        src/render_core/resources/texture.cpp

        # render_core/rhi
        src/render_core/rhi/buffer_resource.cpp
        src/render_core/rhi/command_buffer.cpp
        src/render_core/rhi/compute_pipeline.cpp
        src/render_core/rhi/device_memory_allocator.cpp
        src/render_core/rhi/framebuffer.cpp
        src/render_core/rhi/graphics_pipeline.cpp
        src/render_core/rhi/image_resource.cpp
        src/render_core/rhi/presenter.cpp
        src/render_core/rhi/renderer.cpp
        src/render_core/rhi/render_pass.cpp
        src/render_core/rhi/resource_group.cpp
        src/render_core/rhi/shader.cpp
        src/render_core/rhi/synchronization.cpp

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
        src/render_core/rhi/backend/vulkan/vulkan_presenter.cpp
        src/render_core/rhi/backend/vulkan/vulkan_queue.cpp
        src/render_core/rhi/backend/vulkan/vulkan_render_pass.cpp
        src/render_core/rhi/backend/vulkan/vulkan_resource_group.cpp
        src/render_core/rhi/backend/vulkan/vulkan_shader.cpp
        src/render_core/rhi/backend/vulkan/vulkan_synchronization.cpp
        src/render_core/rhi/backend/vulkan/vulkan_swapchain.cpp
        src/render_core/rhi/backend/vulkan/vulkan_utils.cpp

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
        src/render_core/rhi/backend/opengl/opengl_presenter.cpp
        src/render_core/rhi/backend/opengl/opengl_render_pass.cpp
        src/render_core/rhi/backend/opengl/opengl_resource_group.cpp
        src/render_core/rhi/backend/opengl/opengl_shader.cpp
        src/render_core/rhi/backend/opengl/opengl_synchronization.cpp

        # render_core/abstraction/shader
        src/render_core/shader/shader_reflection.cpp
        src/render_core/shader/shader_transpiler.cpp

        # scene
        src/scene/scene.cpp

        # utility
        src/utility/filesystem.cpp
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
        GIT_TAG v1.13.0
        GIT_PROGRESS TRUE
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
        GIT_TAG vulkan-sdk-1.3.280.0
        GIT_PROGRESS TRUE
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
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb/stb_image.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/stb/)

# entt
FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.2
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(entt)

target_link_libraries(${PROJECT_NAME} PUBLIC EnTT::EnTT)

#
# Plugins
#

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/extensions/)
target_link_libraries(${PROJECT_NAME} PUBLIC MizuExtensions)

#
# Shaders
#

include(cmake/shader_utils.cmake)

set(OUT_SHADER_PATH ${CMAKE_BINARY_DIR}/shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_PATH="${OUT_SHADER_PATH}")

# TODO: Look to deprecate
function(mizu_compile_shader target shader_path output_path base_path)
    get_filename_component(shader_id ${shader_path} NAME)
    get_filename_component(shader_name ${shader_path} NAME_WLE)
    get_filename_component(shader_ext ${shader_path} LAST_EXT)
    get_filename_component(folder_path ${shader_path} PATH)

    string(REPLACE "${base_path}" "" relative_out_path "${folder_path}")

    set(shader_identifier vert)
    if (${shader_ext} STREQUAL ".frag")
        set(shader_identifier frag)
    elseif (${shader_ext} STREQUAL ".comp")
        set(shader_identifier comp)
    endif ()

    file(MAKE_DIRECTORY "${output_path}/${relative_out_path}")
    set(shader_output_path "${output_path}/${relative_out_path}/${shader_name}.${shader_identifier}.spv")

    mizu_compile_glsl_shader(${target} ${shader_path} ${shader_output_path})
endfunction()
# ==============================================

file(MAKE_DIRECTORY ${OUT_SHADER_PATH})

# Deferred 
set(OUT_DEFERRED_SHADER_PATH ${OUT_SHADER_PATH}/deferred)
file(MAKE_DIRECTORY ${OUT_DEFERRED_SHADER_PATH})

mizu_compile_slang_shader(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/DepthPrePass.slang ${OUT_DEFERRED_SHADER_PATH}/DepthPrePass.vert.spv vertex vsMain)
mizu_compile_slang_shader(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/DepthPrePass.slang ${OUT_DEFERRED_SHADER_PATH}/DepthPrePass.frag.spv frag fsMain)

mizu_compile_slang_shader(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/SimpleColor.slang ${OUT_DEFERRED_SHADER_PATH}/SimpleColor.vert.spv vertex vsMain)
mizu_compile_slang_shader(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred/SimpleColor.slang ${OUT_DEFERRED_SHADER_PATH}/SimpleColor.frag.spv frag fsMain)

#file(GLOB_RECURSE SHADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)
#foreach (SHADER_PATH ${SHADER_FILES})
    #mizu_compile_shader(${PROJECT_NAME} ${SHADER_PATH} ${OUT_SHADER_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
#endforeach ()

