project(MizuLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion)

target_sources(${PROJECT_NAME} PRIVATE
        buffers.cpp
        command_buffer.cpp
        framebuffer.cpp
        graphics_pipeline.cpp
        renderer.cpp
        render_pass.cpp
        shader.cpp
        synchronization.cpp
        texture.cpp

        # shader
        shader/shader_transpiler.cpp

        # utility
        utility/filesystem.cpp

        # backend/vulkan
        backend/vulkan/vulkan_backend.cpp
        backend/vulkan/vulkan_buffer.cpp
        backend/vulkan/vulkan_buffers.cpp
        backend/vulkan/vulkan_command_buffer.cpp
        backend/vulkan/vulkan_context.cpp
        backend/vulkan/vulkan_descriptors.cpp
        backend/vulkan/vulkan_device.cpp
        backend/vulkan/vulkan_framebuffer.cpp
        backend/vulkan/vulkan_graphics_pipeline.cpp
        backend/vulkan/vulkan_image.cpp
        backend/vulkan/vulkan_instance.cpp
        backend/vulkan/vulkan_queue.cpp
        backend/vulkan/vulkan_render_pass.cpp
        backend/vulkan/vulkan_shader.cpp
        backend/vulkan/vulkan_synchronization.cpp
        backend/vulkan/vulkan_texture.cpp
        backend/vulkan/vulkan_utils.cpp

        # backend/opengl
        backend/opengl/opengl_backend.cpp
        backend/opengl/opengl_buffers.cpp
        backend/opengl/opengl_framebuffer.cpp
        backend/opengl/opengl_graphics_pipeline.cpp
        backend/opengl/opengl_render_pass.cpp
        backend/opengl/opengl_shader.cpp
        backend/opengl/opengl_synchronization.cpp
        backend/opengl/opengl_texture.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)

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
target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES} EGL)

# Glad 
add_library(gladLib STATIC ${CMAKE_SOURCE_DIR}/vendor/glad/src/glad.c)
target_include_directories(gladLib PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glad/include/)

target_link_libraries(${PROJECT_NAME} PUBLIC gladLib)
