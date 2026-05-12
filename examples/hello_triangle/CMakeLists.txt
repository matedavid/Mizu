project(Example.HelloTriangle LANGUAGES CXX)

mizu_add_game_package(
    TARGET ${PROJECT_NAME}
    DISPLAY_NAME "HelloTriangle Example"
    ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

mizu_add_shader_module(Example.HelloTriangle.ShaderModule)
target_sources(Example.HelloTriangle.ShaderModule 
    PRIVATE
        hello_triangle_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/
            FILES 
                hello_triangle_shaders.h
)

target_compile_definitions(Example.HelloTriangle.ShaderModule PRIVATE MIZU_HELLO_TRIANGLE_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/")

mizu_game_package_attach_shader_module(${PROJECT_NAME} Example.HelloTriangle.ShaderModule)
