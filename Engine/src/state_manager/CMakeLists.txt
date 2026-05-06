project(Engine.StateManager LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_state_manager_module.h"
    EXPORT_MACRO_NAME "MIZU_STATE_MANAGER_API"
)

mizu_configure_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/state_manager_coordinator.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES
                public/state_manager/base_state_manager.h
                public/state_manager/base_state_manager.inl.cpp
                public/state_manager/state_manager.h
                public/state_manager/state_manager_consumer.h
                public/state_manager/state_manager_coordinator.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_state_manager_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
