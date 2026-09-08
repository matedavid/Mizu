project(Engine.World LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_world_module.h"
    EXPORT_MACRO_NAME "MIZU_WORLD_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_world_module)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Engine.Base
    Engine.RenderCore
    Engine.Asset
    Engine.StateManager
    Engine.Core
    Engine.Render
)

