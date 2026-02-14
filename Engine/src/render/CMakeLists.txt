project(Engine.Render LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/core/buffer_utils.cpp
        private/core/command_utils.cpp
        private/core/image_utils.cpp
        private/core/render_utils.cpp
        private/environment/environment.cpp
        private/material/material.cpp
        private/model/mesh.cpp
        private/render_graph/render_graph.cpp
        private/render_graph/render_graph_builder.cpp
        private/render_graph/render_graph_builder2.cpp
        private/render_graph/render_graph_resource_aliasing2.cpp
        private/render_graph/render_graph_utils.cpp
        private/render_graph/render_graph2.cpp
        private/runtime/game_renderer.cpp
        private/runtime/renderer.cpp
        private/state_manager/camera_state_manager.cpp
        private/state_manager/imgui_state_manager.cpp
        private/state_manager/light_state_manager.cpp
        private/state_manager/renderer_settings_state_manager.cpp
        private/state_manager/state_manager_coordinator.cpp
        private/state_manager/static_mesh_state_manager.cpp
        private/state_manager/transform_state_manager.cpp
        private/systems/pipeline_cache.cpp
        private/systems/sampler_state_cache.cpp
        private/systems/shader_manager.cpp
        private/camera.cpp
        private/draw_block_manager.cpp
        private/render_graph_renderer.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private/
            FILES
                private/render_graph/render_graph_resource_aliasing.h
                private/render_graph/render_graph_resource_aliasing2.h
                private/render_graph_renderer_parameters.h

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/render/core/buffer_utils.h
                public/render/core/command_utils.h
                public/render/core/image_utils.h
                public/render/core/render_utils.h
                public/render/environment/environment.h
                public/render/material/material.h
                public/render/model/mesh.h
                public/render/passes/pass_info.h
                public/render/render_graph/render_graph.h
                public/render/render_graph/render_graph_blackboard.h
                public/render/render_graph/render_graph_builder.h
                public/render/render_graph/render_graph_resources.h
                public/render/render_graph/render_graph_types.h
                public/render/render_graph/render_graph_utils.h
                public/render/render_graph/render_graph_builder2.h
                public/render/render_graph/render_graph_types2.h
                public/render/render_graph/render_graph2.h
                public/render/runtime/game_renderer.h
                public/render/runtime/renderer.h
                public/render/state_manager/camera_state_manager.h
                public/render/state_manager/imgui_state_manager.h
                public/render/state_manager/light_state_manager.h
                public/render/state_manager/renderer_settings_state_manager.h
                public/render/state_manager/state_manager_coordinator.h
                public/render/state_manager/static_mesh_state_manager.h
                public/render/state_manager/transform_state_manager.h
                public/render/systems/pipeline_cache.h
                public/render/systems/sampler_state_cache.h
                public/render/systems/shader_manager.h
                public/render/camera.h
                public/render/draw_block_manager.h
                public/render/render_graph_renderer.h
                public/render/render_graph_renderer_settings.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore.Dx12)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore.Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Shader)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Core)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.StateManager)

target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_ENGINE_SHADERS_PATH="${MIZU_ENGINE_SHADERS_OUTPUT_PATH}")

# Pipeline
shader_compilation_pipeline_create_module(Engine.Render.Pipeline)
target_sources(Engine.Render.Pipeline 
    PRIVATE
        pipeline/render_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pipeline/
            FILES 
                pipeline/render.pipeline/material_shaders.h
                pipeline/render.pipeline/render_graph_renderer_shaders.h
)

target_compile_definitions(Engine.Render.Pipeline PRIVATE MIZU_ENGINE_SHADERS_SOURCE_PATH="${MIZU_ENGINE_SHADERS_SOURCE_PATH}")

target_link_libraries(${PROJECT_NAME} PUBLIC $<LINK_LIBRARY:WHOLE_ARCHIVE,Engine.Render.Pipeline>)

#
# Dependencies
#

# stb
add_library(mizu_stb STATIC)
target_sources(mizu_stb PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image.cpp)
target_include_directories(mizu_stb PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb/)

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

