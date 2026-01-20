project(MizuEngineLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

include(cmake/CommonConfig.cmake)

set(MIZU_ENGINE_SHADERS_PATH ${CMAKE_BINARY_DIR}/shaders)
set(MIZU_ENGINE_SHADERS_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_include_directories(${PROJECT_NAME} PUBLIC include/)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/base/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core.dx12/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core.vulkan/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/state_manager/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    Engine.Base
    Engine.RenderCore
    Engine.RenderCore.Dx12
    Engine.RenderCore.Vulkan
    Engine.StateManager
    Engine.Core
    Engine.Render
    Engine.Runtime
)

add_library(MizuEngine)
target_link_libraries(MizuEngine PUBLIC ${PROJECT_NAME})

add_dependencies(MizuEngine MizuShaderPipeline)

#
# Tools
#

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools/)

#
# Shaders
#

mizu_shader_pipeline_add_shader_provider_define(${CMAKE_CURRENT_SOURCE_DIR}/src/render/private/render_graph_renderer_shaders_provider.cpp MIZU_ENGINE_SHADERS_SOURCE_PATH="${MIZU_ENGINE_SHADERS_SOURCE_PATH}")
mizu_shader_pipeline_add_shader_provider(${CMAKE_CURRENT_SOURCE_DIR}/src/render/private/material/material_shader_provider.cpp)
mizu_shader_pipeline_add_shader_provider(${CMAKE_CURRENT_SOURCE_DIR}/src/render/private/environment/environment_shader_provider.cpp)

#
# Extensions
#

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/extensions/)
target_link_libraries(${PROJECT_NAME} PUBLIC MizuExtensions)
