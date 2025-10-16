project(MizuTools C CXX)

add_executable(MizuShaderPipeline)
target_sources(MizuShaderPipeline PRIVATE 
	shader_pipeline/shader_pipeline.cpp
)

target_include_directories(MizuShaderPipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shader_pipeline/)
target_link_libraries(MizuShaderPipeline PRIVATE MizuLib)

target_compile_definitions(MizuShaderPipeline PRIVATE MIZU_ENGINE_SHADERS_SOURCE_PATH="${MIZU_ENGINE_SHADERS_SOURCE_PATH}")
target_compile_definitions(MizuShaderPipeline PRIVATE MIZU_ENGINE_SHADERS_OUTPUT_PATH="${MIZU_ENGINE_SHADERS_PATH}")

function(mizu_shader_pipeline_add_shader_provider shader_provider_path)
	target_sources(MizuShaderPipeline PRIVATE ${shader_provider_path})
endfunction()

function(mizu_shader_pipeline_add_shader_provider_define shader_provider_path define)
	mizu_shader_pipeline_add_shader_provider(${shader_provider_path})
	target_compile_definitions(MizuShaderPipeline PRIVATE ${define})
endfunction()

