project(MizuTools C CXX)

add_executable(MizuShaderPipeline)
target_sources(MizuShaderPipeline PRIVATE 
	shader_pipeline/shader_pipeline.cpp
)

target_include_directories(MizuShaderPipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shader_pipeline/)
target_link_libraries(MizuShaderPipeline PRIVATE MizuEngineLib)

target_compile_definitions(MizuShaderPipeline PRIVATE MIZU_ENGINE_SHADERS_OUTPUT_PATH="${MIZU_ENGINE_SHADERS_PATH}")

set(MIZU_SHADER_PIPELINE_EXECUTABLE_PATH "${CMAKE_CURRENT_BINARY_DIR}/MizuShaderPipeline.exe")

add_custom_command(
    TARGET MizuShaderPipeline
    POST_BUILD 
    COMMAND ${MIZU_SHADER_PIPELINE_EXECUTABLE_PATH}
    COMMENT "Compiling Shaders"
)

function(mizu_shader_pipeline_add_shader_provider shader_provider_path)
	target_sources(MizuShaderPipeline PRIVATE ${shader_provider_path})
endfunction()

function(mizu_shader_pipeline_add_shader_provider_define shader_provider_path)
    mizu_shader_pipeline_add_shader_provider(${shader_provider_path})
    if (ARGN)
        target_compile_definitions(MizuShaderPipeline PRIVATE ${ARGN})
    endif()
endfunction()

