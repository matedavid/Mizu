project(MizuEngineLib LANGUAGES C CXX)

add_library(${PROJECT_NAME})

include(cmake/CommonConfig.cmake)

set(MIZU_ENGINE_SHADERS_PATH ${CMAKE_BINARY_DIR}/shaders)
set(MIZU_ENGINE_SHADERS_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

target_sources(${PROJECT_NAME} PRIVATE
        # application
        src/application/application.cpp

        # scene
        src/scene/scene.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_link_libraries(${PROJECT_NAME} PUBLIC MizuProjectOptions)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/base/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core.dx12/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core.vulkan/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/state_manager/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render/)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    Engine.Base
    Engine.RenderCore
    Engine.RenderCore.Dx12
    Engine.RenderCore.Vulkan
    Engine.StateManager
    Engine.Core
    Engine.Render
)

add_library(MizuEngine)
target_link_libraries(MizuEngine PUBLIC ${PROJECT_NAME})

add_dependencies(MizuEngine MizuShaderPipeline)

add_library(MizuEngineWithMain)
target_sources(MizuEngineWithMain PRIVATE
        src/runtime/entry_point.cpp
        src/runtime/main_loop.cpp
)

target_link_libraries(MizuEngineWithMain PUBLIC MizuEngine)

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

#
# Dependencies
#

# entt
FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.2
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        SYSTEM
)
FetchContent_MakeAvailable(entt)

target_link_libraries(${PROJECT_NAME} PUBLIC EnTT::EnTT)

