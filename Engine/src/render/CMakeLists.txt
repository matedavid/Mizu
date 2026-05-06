project(Engine.Render LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_render_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_API"
)

mizu_configure_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/core/camera.cpp
        private/material/material.cpp
        private/model/mesh.cpp
        private/render_graph/render_graph_builder.cpp
        private/render_graph/render_graph_resource_aliasing.cpp
        private/render_graph/render_graph_resource_registry.cpp
        private/render_graph/render_graph.cpp
        private/runtime/game_renderer.cpp
        private/runtime/render_loop.cpp
        private/runtime/renderer.cpp
        private/state_manager/camera_state_manager.cpp
        private/state_manager/light_state_manager.cpp
        private/state_manager/renderer_settings_state_manager.cpp
        private/state_manager/static_mesh_state_manager.cpp
        private/state_manager/transform_state_manager.cpp
        private/systems/pipeline_cache.cpp
        private/systems/sampler_state_cache.cpp
        private/systems/shader_manager.cpp
        private/utils/buffer_utils.cpp
        private/utils/command_utils.cpp
        private/utils/image_utils.cpp
        private/utils/render_utils.cpp
        private/draw_block_manager.cpp
        private/frame_linear_allocator.cpp
        private/light_manager.cpp
        private/mesh_manager.cpp
        private/render_graph_renderer.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private/
            FILES
                private/render_graph/render_graph_resource_aliasing.h
                private/light_manager.h
                private/mesh_manager.h

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES
                public/render/core/camera.h
                public/render/core/lights.h
                public/render/material/material.h
                public/render/model/mesh.h
                public/render/passes/pass_info.h
                public/render/render_graph/render_graph_blackboard.h
                public/render/render_graph/render_graph_builder.h
                public/render/render_graph/render_graph_resource_registry.h
                public/render/render_graph/render_graph_types.h
                public/render/render_graph/render_graph.h
                public/render/runtime/game_renderer.h
                public/render/runtime/render_loop.h
                public/render/runtime/renderer.h
                public/render/state_manager/camera_state_manager.h
                public/render/state_manager/light_state_manager.h
                public/render/state_manager/renderer_settings_state_manager.h
                public/render/state_manager/static_mesh_state_manager.h
                public/render/state_manager/transform_state_manager.h
                public/render/systems/pipeline_cache.h
                public/render/systems/sampler_state_cache.h
                public/render/systems/shader_manager.h
                public/render/utils/buffer_utils.h
                public/render/utils/command_utils.h
                public/render/utils/image_utils.h
                public/render/utils/render_utils.h
                public/render/draw_block_manager.h
                public/render/frame_linear_allocator.h
                public/render/render_graph_renderer.h
                public/render/render_graph_renderer_settings.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
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
set_target_properties(mizu_stb PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

