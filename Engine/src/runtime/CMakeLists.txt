project(Engine.Runtime LANGUAGES CXX)

# TODO: Not sure I like only having Engine.Runtime as a static library while the others are shared...
add_library(${PROJECT_NAME} STATIC)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_runtime_module.h"
    EXPORT_MACRO_NAME "MIZU_RUNTIME_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/application.cpp
        private/entry_point.cpp
        private/main_loop.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/runtime/application.h
                public/runtime/game_main.h
                public/runtime/game_simulation.h
                public/runtime/layer.h
                public/runtime/main_loop.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_runtime_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Core)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.StateManager)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Shader)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Render)
