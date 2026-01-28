project(MizuEngine LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

include(cmake/CommonConfig.cmake)

set(MIZU_ENGINE_SHADERS_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(MIZU_ENGINE_SHADERS_OUTPUT_PATH ${CMAKE_BINARY_DIR}/shaders)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools/)

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/base/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core.dx12/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render_core.vulkan/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/state_manager/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/core/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/shader/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/render/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/)

target_link_libraries(${PROJECT_NAME} INTERFACE 
    Engine.Base
    Engine.RenderCore
    Engine.RenderCore.Dx12
    Engine.RenderCore.Vulkan
    Engine.StateManager
    Engine.Core
    Engine.Shader
    Engine.Render
    Engine.Runtime
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/extensions/)

target_link_libraries(${PROJECT_NAME} INTERFACE 
    Engine.Extension.AssimpLoader
    Engine.Extension.CameraControllers
)

# add_dependencies(MizuEngine MizuShaderPipeline)

