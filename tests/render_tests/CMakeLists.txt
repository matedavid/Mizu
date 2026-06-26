project(Test.RenderTests)

add_executable(${PROJECT_NAME})

mizu_configure_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        runner/render_tests_registry.cpp
        runner/render_tests_runner.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/
            FILES
                runner/render_test.h
                runner/render_tests_registry.h
)

target_sources(${PROJECT_NAME} 
    PRIVATE
        tests/hello_triangle/hello_triangle_render_test.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine.Base
    Engine.RenderCore
    Engine.Shader
    Engine.Render
    mizu_stb
)

# Pipeline
mizu_configure_shader_pipeline(Test.RenderTests.Pipeline)

mizu_add_shader_module(Test.RenderTests.ShaderModule)
target_sources(Test.RenderTests.ShaderModule
    PRIVATE
        pipeline/render_tests_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pipeline/
            FILES
                pipeline/render_tests.pipeline/hello_triangle_shaders.h
)

set(RENDER_TESTS_SHADERS_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
target_compile_definitions(Test.RenderTests.ShaderModule PRIVATE RENDER_TESTS_SHADERS_SOURCE_PATH="${RENDER_TESTS_SHADERS_SOURCE_PATH}")

target_link_libraries(${PROJECT_NAME} PUBLIC Test.RenderTests.ShaderModule)
mizu_attach_shader_module(Test.RenderTests.Pipeline Test.RenderTests.ShaderModule)