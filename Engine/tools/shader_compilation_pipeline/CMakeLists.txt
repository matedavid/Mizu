project(ShaderCompilationPipeline LANGUAGES CXX)

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)

target_sources(${PROJECT_NAME} 
	PRIVATE 
		private/shader_compilation_pipeline.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Shader)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Render)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_OUTPUT_PATH="${MIZU_ENGINE_SHADERS_OUTPUT_PATH}")

function (shader_compilation_pipeline_create_module target_name)
	add_library(${target_name} STATIC)

	target_link_libraries(${target_name} PRIVATE MizuProjectOptions Engine.Base Engine.RenderCore Engine.Shader)
	set_target_properties(${target_name} PROPERTIES UNITY_BUILD ON)

	target_link_libraries(ShaderCompilationPipeline PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${target_name}>)
endfunction ()
