project(Engine.Asset LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_asset_module.h"
    EXPORT_MACRO_NAME "MIZU_ASSET_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_asset_module)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)

