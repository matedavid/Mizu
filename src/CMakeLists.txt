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

        # backend/vulkan
        backend/vulkan/vulkan_backend.cpp
        backend/vulkan/vulkan_buffer.cpp
        backend/vulkan/vulkan_buffers.cpp
        backend/vulkan/vulkan_command_buffer.cpp
        backend/vulkan/vulkan_context.cpp
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
        GIT_TAG vulkan-sdk-1.3.275.0
        GIT_PROGRESS TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spirv-reflect)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)

# Glad 
FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG c
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glad)

add_library(gladLib STATIC ${glad_SOURCE_DIR}/src/glad.c)
target_include_directories(gladLib PUBLIC ${glad_SOURCE_DIR}/include/)

target_link_libraries(${PROJECT_NAME} PRIVATE gladLib)
