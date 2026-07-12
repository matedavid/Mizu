project(Engine.Render LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_render_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_render_module)

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
                pipeline/render.pipeline/scene_renderer_shaders.h
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

