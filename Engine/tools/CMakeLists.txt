project(MizuTools C CXX)

option(MIZU_SHADER_PIPELINE_ALWAYS_EXECUTE "Run ShaderPipeline always, even when not invoked directly or MizuShaderPipeline target has changed" TRUE)

add_executable(MizuShaderPipeline)
target_sources(MizuShaderPipeline PRIVATE 
	shader_pipeline/shader_pipeline.cpp
)

target_include_directories(MizuShaderPipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shader_pipeline/)
target_link_libraries(MizuShaderPipeline PRIVATE MizuEngineLib)
target_link_libraries(MizuShaderPipeline PRIVATE MizuProjectOptions)

target_compile_definitions(MizuShaderPipeline PRIVATE MIZU_ENGINE_SHADERS_OUTPUT_PATH="${MIZU_ENGINE_SHADERS_PATH}")

set(MIZU_SHADER_PIPELINE_EXECUTABLE_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MizuShaderPipeline.exe")

if (MIZU_SHADER_PIPELINE_ALWAYS_EXECUTE)
    add_custom_target(MizuShaderPipelineCompileShaders ALL DEPENDS MizuShaderPipelineDummyCommandAlwaysExecutes)

    add_custom_command(
        OUTPUT MizuShaderPipelineDummyCommandAlwaysExecutes 
        COMMAND cmake -E echo
    )

    add_dependencies(MizuShaderPipelineCompileShaders MizuShaderPipeline)
    add_dependencies(MizuEngine MizuShaderPipelineCompileShaders)

    add_custom_command(
        TARGET MizuShaderPipelineCompileShaders
        POST_BUILD 
        COMMAND ${MIZU_SHADER_PIPELINE_EXECUTABLE_PATH}
        COMMENT "Compiling Shaders"
    )
else ()
    add_custom_command(
        TARGET MizuShaderPipeline 
        POST_BUILD 
        COMMAND ${MIZU_SHADER_PIPELINE_EXECUTABLE_PATH}
        COMMENT "Compiling Shaders"
    )
endif()

function(mizu_shader_pipeline_add_shader_provider shader_provider_path)
	target_sources(MizuShaderPipeline PRIVATE ${shader_provider_path})
endfunction()

function(mizu_shader_pipeline_add_shader_provider_define shader_provider_path)
    mizu_shader_pipeline_add_shader_provider(${shader_provider_path})
    if (ARGN)
        target_compile_definitions(MizuShaderPipeline PRIVATE ${ARGN})
    endif()
endfunction()

