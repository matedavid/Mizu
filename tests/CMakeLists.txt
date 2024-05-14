project(MizuTests LANGUAGES C CXX)

add_executable(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_TEST_RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources")

target_sources(${PROJECT_NAME} PRIVATE
        main.cpp
        resources_manager.cpp

        buffers_tests.cpp
        command_buffer_tests.cpp
        framebuffer_tests.cpp
        graphics_pipeline_tests.cpp
        initialization_tests.cpp
        render_pass_tests.cpp
        resource_group_tests.cpp
        shader_tests.cpp
        texture_tests.cpp
)

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
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2)
