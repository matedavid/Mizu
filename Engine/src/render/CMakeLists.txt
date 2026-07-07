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
        private/registries/light_registry.cpp
        private/registries/renderable_registry.cpp
        private/render_graph/render_graph_builder.cpp
        private/render_graph/render_graph_resource_aliasing.cpp
        private/render_graph/render_graph_resource_registry.cpp
        private/render_graph/render_graph.cpp
        private/resources/asset_load_system.cpp
        private/resources/cpu_loading_pool.cpp
        private/resources/gpu_pools.cpp
        private/resources/residency_system.cpp
        private/resources/resource_event_stream.cpp
        private/resources/streaming_planner.cpp
        private/runtime/game_renderer.cpp
        private/runtime/render_loop.cpp
        private/runtime/renderer.cpp
        private/scene/draw_list_system.cpp
        private/scene/scene_system.cpp
        private/state_manager/camera_state_manager.cpp
        private/state_manager/light_state_manager.cpp
        private/state_manager/renderer_settings_state_manager.cpp
        private/state_manager/static_mesh_state_manager.cpp
        private/state_manager/transform_state_manager.cpp
        private/systems/frame_linear_allocator.cpp
        private/systems/pipeline_cache.cpp
        private/systems/sampler_state_cache.cpp
        private/systems/shader_manager.cpp
        private/utils/buffer_utils.cpp
        private/utils/command_utils.cpp
        private/utils/image_utils.cpp
        private/render_graph_renderer.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private/
            FILES
                private/registries/light_registry.h
                private/registries/renderable_registry.h
                private/render_graph/render_graph_resource_aliasing.h
                private/resources/asset_load_system.h
                private/resources/cpu_loading_pool.h
                private/resources/gpu_pools.h
                private/resources/residency_system.h
                private/resources/resource_event_stream.h
                private/resources/streaming_planner.h
                private/scene/scene_system.h

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES
                public/render/core/camera.h
                public/render/core/lights.h
                public/render/passes/pass_info.h
                public/render/render_graph/render_graph_blackboard.h
                public/render/render_graph/render_graph_builder.h
                public/render/render_graph/render_graph_resource_registry.h
                public/render/render_graph/render_graph_types.h
                public/render/render_graph/render_graph.h
                public/render/resources/gpu_resource_types.h
                public/render/runtime/game_renderer.h
                public/render/runtime/render_loop.h
                public/render/runtime/renderer.h
                public/render/scene/draw_list_raster_pass.h
                public/render/scene/draw_list_system.h
                public/render/scene/draw_list_system_types.h
                public/render/state_manager/camera_state_manager.h
                public/render/state_manager/light_state_manager.h
                public/render/state_manager/renderer_settings_state_manager.h
                public/render/state_manager/static_mesh_state_manager.h
                public/render/state_manager/transform_state_manager.h
                public/render/systems/frame_linear_allocator.h
                public/render/systems/pipeline_cache.h
                public/render/systems/sampler_state_cache.h
                public/render/systems/shader_manager.h
                public/render/utils/buffer_utils.h
                public/render/utils/command_utils.h
                public/render/utils/image_utils.h
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
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Asset)

target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_ENGINE_SHADERS_PATH="${MIZU_ENGINE_SHADERS_OUTPUT_PATH}")

# Pipeline
mizu_add_shader_module(Engine.Render.ShaderModule)
target_sources(Engine.Render.ShaderModule
    PRIVATE
        pipeline/render_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pipeline/
            FILES
                pipeline/render.pipeline/material_shaders.h
                pipeline/render.pipeline/render_graph_renderer_shaders.h
                pipeline/render.pipeline/scene_shaders.h
)

target_compile_definitions(Engine.Render.ShaderModule PRIVATE MIZU_ENGINE_SHADERS_SOURCE_PATH="${MIZU_ENGINE_SHADERS_SOURCE_PATH}")

target_link_libraries(${PROJECT_NAME} PUBLIC Engine.Render.ShaderModule)
mizu_attach_shader_module(Mizu.Engine.Pipeline Engine.Render.ShaderModule)

#
# Dependencies
#

# stb
if (NOT TARGET mizu_stb)
    add_library(mizu_stb STATIC)
    target_sources(mizu_stb PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image.cpp ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image_write.cpp)
    target_include_directories(mizu_stb PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb/)
    set_target_properties(mizu_stb PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

