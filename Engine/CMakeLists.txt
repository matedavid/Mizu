project(MizuEngine LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

include(cmake/CommonConfig.cmake)
include(cmake/GamePackage.cmake)

option(MIZU_BUILD_DX12   "Enable DirectX12 support" ON)
option(MIZU_BUILD_VULKAN "Enable Vulkan support"    ON)

set(MIZU_ENGINE_SHADERS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools/)

target_link_libraries(${PROJECT_NAME} INTERFACE MizuEngineDefines)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/base/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/state_manager/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/asset/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/shader/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/world/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/)

target_link_libraries(${PROJECT_NAME} INTERFACE 
    Engine.Base
    Engine.RenderCore
    Engine.StateManager
    Engine.Asset
    Engine.Core
    Engine.Shader
    Engine.Render
    Engine.World
    Engine.Runtime
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/extensions/)

target_link_libraries(${PROJECT_NAME} INTERFACE 
    $<TARGET_NAME_IF_EXISTS:Engine.Extension.CameraControllers>
    $<TARGET_NAME_IF_EXISTS:Engine.Extension.ImGui>
    $<TARGET_NAME_IF_EXISTS:Engine.Extension.ImGui.RenderModule>
)

# add_dependencies(MizuEngine MizuShaderPipeline)

