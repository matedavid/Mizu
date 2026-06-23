project(ShaderCompilationPipeline LANGUAGES CXX)

add_library(${PROJECT_NAME} OBJECT)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuEngineDefines MizuPrivateOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD} POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME} PRIVATE
        private/shader_compilation_pipeline.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Engine.Base
        Engine.RenderCore
        Engine.Shader
        Engine.Render
)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_OUTPUT_PATH="${MIZU_ENGINE_SHADERS_OUTPUT_PATH}")

function(mizu_configure_shader_pipeline target_name)
    add_executable(${target_name} $<TARGET_OBJECTS:ShaderCompilationPipeline>)
    target_link_libraries(${target_name} PRIVATE
            Engine.Base
            Engine.RenderCore
            Engine.Shader
            Engine.Render
    )

    target_link_libraries(${target_name} PRIVATE MizuEngineDefines MizuPrivateOptions)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})
endfunction()

function(mizu_add_shader_module target_name)
    add_library(${target_name} STATIC)

    target_link_libraries(${target_name} PRIVATE MizuEngineDefines MizuPrivateOptions Engine.Base Engine.RenderCore Engine.Shader)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD} POSITION_INDEPENDENT_CODE ON)
endfunction()

function(mizu_attach_shader_module pipeline_target module_target)
    target_link_libraries(${pipeline_target} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${module_target}>)
endfunction()

