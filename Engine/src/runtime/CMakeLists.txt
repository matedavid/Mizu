project(Engine.Runtime LANGUAGES CXX)

# TODO: Not sure I like only having Engine.Runtime as a static library while the others are shared...
add_library(${PROJECT_NAME} STATIC)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_runtime_module.h"
    EXPORT_MACRO_NAME "MIZU_RUNTIME_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_runtime_module)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Core)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.StateManager)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Asset)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Shader)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Render)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Package)

set(MIZU_RUNTIME_ENTRY_POINT "${CMAKE_CURRENT_SOURCE_DIR}/entry/runtime_main.cpp" CACHE INTERNAL "Engine runtime entry point source")
