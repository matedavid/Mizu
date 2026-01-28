project(Engine.Extension.CameraControllers LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_camera_controllers_module.h"
    EXPORT_MACRO_NAME "MIZU_CAMERA_CONTROLLERS_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/editor_camera_controller.cpp
        private/first_person_camera_controller.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/camera_controllers/editor_camera_controller.h
                public/camera_controllers/first_person_camera_controller.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_camera_controllers_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base Engine.RenderCore Engine.Render Engine.Core)
