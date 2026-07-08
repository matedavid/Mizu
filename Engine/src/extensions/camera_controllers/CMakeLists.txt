project(Engine.Extension.CameraControllers LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_camera_controllers_module.h"
    EXPORT_MACRO_NAME "MIZU_CAMERA_CONTROLLERS_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_camera_controllers_module)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base Engine.RenderCore Engine.Render Engine.Core)
