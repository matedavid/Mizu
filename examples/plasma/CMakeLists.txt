project(Example.Plasma LANGUAGES CXX)

mizu_add_game_package(
    TARGET ${PROJECT_NAME}
    DISPLAY_NAME "Plasma Example"
    ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

mizu_add_shader_module(Example.Plasma.ShaderModule)
target_sources(Example.Plasma.ShaderModule 
    PRIVATE
        plasma_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/
            FILES 
                plasma_shaders.h
)

target_compile_definitions(Example.Plasma.ShaderModule PRIVATE MIZU_PLASMA_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

mizu_game_package_attach_shader_module(${PROJECT_NAME} Example.Plasma.ShaderModule)
