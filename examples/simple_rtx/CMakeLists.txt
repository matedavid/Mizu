project(Example.SimpleRtx LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE MizuEngine)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

shader_compilation_pipeline_create_module(Example.SimpleRtx.Pipeline)
target_sources(Example.SimpleRtx.Pipeline 
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

target_compile_definitions(Example.SimpleRtx.Pipeline PRIVATE MIZU_SIMPLE_RTX_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

target_link_libraries(${PROJECT_NAME} PUBLIC $<LINK_LIBRARY:WHOLE_ARCHIVE,Example.SimpleRtx.Pipeline>)
