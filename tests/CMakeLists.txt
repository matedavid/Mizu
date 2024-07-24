project(MizuTests LANGUAGES C CXX)

# Options
option(MIZU_TESTS_BUILD_RENDERER_TESTS "Build Renderer tests" ON)

add_executable(${PROJECT_NAME})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion)
endif()

file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources" TEST_RESOURCES_PATH)
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_TEST_RESOURCES_PATH="${TEST_RESOURCES_PATH}")

target_sources(${PROJECT_NAME} PRIVATE
        resources_manager.cpp
)

# Renderer
if (MIZU_TESTS_BUILD_RENDERER_TESTS)
    target_sources(${PROJECT_NAME} PRIVATE
            renderer/buffers_tests.cpp
            renderer/command_buffer_tests.cpp
            renderer/framebuffer_tests.cpp
            renderer/graphics_pipeline_tests.cpp
            renderer/initialization_tests.cpp
            renderer/render_pass_tests.cpp
            renderer/resource_group_tests.cpp
            renderer/shader_tests.cpp
            renderer/texture_tests.cpp
    )
endif ()

# Scene
target_sources(${PROJECT_NAME} PRIVATE
        scene/entity.cpp
        scene/scene.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE MizuLib)

#
# Dependencies
#
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(Catch2)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)
