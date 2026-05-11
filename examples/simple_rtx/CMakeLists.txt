project(Example.SimpleRtx LANGUAGES CXX)

mizu_add_game_package(
    TARGET ${PROJECT_NAME}
    DISPLAY_NAME "SimpleRtx Example"
    ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

mizu_add_shader_module(Example.SimpleRtx.ShaderModule)
target_sources(Example.SimpleRtx.ShaderModule 
    PRIVATE
        simple_rtx_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/
            FILES 
                copy_shaders.h
                simple_rtx_shaders.h
)

target_compile_definitions(Example.SimpleRtx.ShaderModule PRIVATE MIZU_SIMPLE_RTX_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

target_link_libraries(${PROJECT_NAME} PUBLIC $<LINK_LIBRARY:WHOLE_ARCHIVE,Example.SimpleRtx.ShaderModule>)
mizu_attach_shader_module(${PROJECT_NAME}.Pipeline Example.SimpleRtx.ShaderModule)
