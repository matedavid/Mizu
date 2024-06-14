project(MizuLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion)
endif()

target_sources(${PROJECT_NAME} PRIVATE
        # core
        src/core/application.cpp
        src/core/window.cpp

        # input
        src/input/input.cpp

        # renderer
        src/renderer/camera.cpp

        # renderer/abstraction
        src/renderer/abstraction/buffers.cpp
        src/renderer/abstraction/command_buffer.cpp
        src/renderer/abstraction/framebuffer.cpp
        src/renderer/abstraction/graphics_pipeline.cpp
        src/renderer/abstraction/presenter.cpp
        src/renderer/abstraction/renderer.cpp
        src/renderer/abstraction/render_pass.cpp
        src/renderer/abstraction/resource_group.cpp
        src/renderer/abstraction/shader.cpp
        src/renderer/abstraction/synchronization.cpp
        src/renderer/abstraction/texture.cpp

        # renderer/abstraction/shader
        src/renderer/abstraction/shader/shader_transpiler.cpp

        # renderer/abstraction/backend/vulkan
        src/renderer/abstraction/backend/vulkan/vulkan_backend.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_buffer.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_buffers.cpp
        src/renderer/abstraction/backend/vulkan/vulkan_command_buffer.cpp
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
        src/renderer/abstraction/backend/opengl/opengl_context.cpp
        src/renderer/abstraction/backend/opengl/opengl_framebuffer.cpp
        src/renderer/abstraction/backend/opengl/opengl_graphics_pipeline.cpp
        src/renderer/abstraction/backend/opengl/opengl_presenter.cpp
        src/renderer/abstraction/backend/opengl/opengl_render_pass.cpp
        src/renderer/abstraction/backend/opengl/opengl_resource_group.cpp
        src/renderer/abstraction/backend/opengl/opengl_shader.cpp
        src/renderer/abstraction/backend/opengl/opengl_synchronization.cpp
        src/renderer/abstraction/backend/opengl/opengl_texture.cpp

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
        GIT_TAG 1.0.1
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glm)

# TODO: Not sure if good idea
# target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE) # Only for Vulkan

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

# Spirv-reflect
FetchContent_Declare(
        spirv-reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG vulkan-sdk-1.3.280.0
        GIT_PROGRESS TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spirv-reflect)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)

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
add_library(gladLib STATIC ${CMAKE_SOURCE_DIR}/vendor/glad/src/glad.c)
target_include_directories(gladLib PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glad/include/)

target_link_libraries(${PROJECT_NAME} PUBLIC gladLib)
