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

        # renderer
        src/renderer/camera.cpp
        src/renderer/mesh.cpp
        src/renderer/primitive_factory.cpp

        # renderer/abstraction
        src/renderer/abstraction/buffers.cpp
        src/renderer/abstraction/command_buffer.cpp
        src/renderer/abstraction/compute_pipeline.cpp
        src/renderer/abstraction/cubemap.cpp
        src/renderer/abstraction/framebuffer.cpp
        src/renderer/abstraction/graphics_pipeline.cpp
        src/renderer/abstraction/image.cpp
        src/renderer/abstraction/presenter.cpp
        src/renderer/abstraction/renderer.cpp
        src/renderer/abstraction/render_pass.cpp
        src/renderer/abstraction/resource_group.cpp
        src/renderer/abstraction/shader.cpp
        src/renderer/abstraction/synchronization.cpp
        src/renderer/abstraction/texture.cpp

        # renderer/abstraction/backend/vulkan
        src/renderer/abstraction/backend/vulkan/vulkan_backend.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_buffer.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_buffers.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_command_buffer.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_compute_pipeline.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_cubemap.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_context.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_descriptors.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_device.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_framebuffer.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_image.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_instance.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_presenter.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_queue.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_render_pass.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_resource_group.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_shader.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_synchronization.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_swapchain.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_texture.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_utils.cpp

        # renderer/abstraction/backend/opengl
        src/renderer/abstraction/backend/opengl/opengl_backend.cpp
        src/renderer/abstraction/backend/opengl/opengl_buffers.cpp
        src/renderer/abstraction/backend/opengl/opengl_command_buffer.cpp
        src/renderer/abstraction/backend/opengl/opengl_compute_pipeline.cpp
        src/renderer/abstraction/backend/opengl/opengl_cubemap.cpp
        src/renderer/abstraction/backend/opengl/opengl_context.cpp
        src/renderer/abstraction/backend/opengl/opengl_framebuffer.cpp
        src/renderer/abstraction/backend/opengl/opengl_graphics_pipeline.cpp
        src/renderer/abstraction/backend/opengl/opengl_image.cpp
        src/renderer/abstraction/backend/opengl/opengl_presenter.cpp
        src/renderer/abstraction/backend/opengl/opengl_render_pass.cpp
        src/renderer/abstraction/backend/opengl/opengl_resource_group.cpp
        src/renderer/abstraction/backend/opengl/opengl_shader.cpp
        src/renderer/abstraction/backend/opengl/opengl_synchronization.cpp
        src/renderer/abstraction/backend/opengl/opengl_texture.cpp

        # renderer/render_graph
        src/renderer/render_graph/render_graph.cpp
        src/renderer/render_graph/render_graph_builder.cpp
        src/renderer/render_graph/render_graph_dependencies.cpp

        # renderer/abstraction/shader
        src/renderer/shader/shader_reflection.cpp
        src/renderer/shader/shader_transpiler.cpp

        # renderer/system
        src/renderer/system/forward_renderer.cpp

        # scene
        src/scene/scene.cpp

        # utility
        src/utility/filesystem.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_include_directories(${PROJECT_NAME} PUBLIC include/)

# Plugins
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/)
target_link_libraries(${PROJECT_NAME} PUBLIC MizuPlugins)

#
# Shaders
#

find_program(GLSLANG_COMPILER NAMES "glslang" "glslang.exe")
find_program(GLSLC_COMPILER NAMES "glslc" "glslc.exe")

if ((NOT GLSLANG_COMPILER) AND (NOT GLSLC_COMPILER))
    message(FATAL_ERROR "No shader compilers found on system")
endif ()

function(mizu_compile_shader_command shader_path shader_output_path out_command)
    if (GLSLANG_COMPILER)
        set(${out_command} ${GLSLANG_COMPILER} ${shader_path} -o ${shader_output_path} -V PARENT_SCOPE)
    elseif (GLSLC_COMPILER)
        set(${out_command} ${GLSLC_COMPILER} ${shader_path} -o ${shader_output_path} PARENT_SCOPE)
    endif ()
endfunction()

set(OUT_SHADER_PATH ${CMAKE_BINARY_DIR}/shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_PATH="${OUT_SHADER_PATH}")

function(mizu_compile_shader shader_path output_path base_path)
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

    mizu_compile_shader_command(${shader_path} ${shader_output_path} compile_command)
    add_custom_command(
            OUTPUT ${shader_output_path}
            COMMAND ${compile_command}
            DEPENDS ${shader_path}
            COMMENT "Compiling shader ${shader_id}"
    )

    add_custom_target(${shader_id} DEPENDS ${shader_output_path})
    add_dependencies(${PROJECT_NAME} ${shader_id})
endfunction()

file(MAKE_DIRECTORY ${OUT_SHADER_PATH})

file(GLOB_RECURSE SHADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)
foreach (SHADER_PATH ${SHADER_FILES})
    mizu_compile_shader(${SHADER_PATH} ${OUT_SHADER_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
endforeach ()

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
