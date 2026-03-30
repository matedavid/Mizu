project(Engine.StateManager LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_state_manager_module.h"
    EXPORT_MACRO_NAME "MIZU_STATE_MANAGER_API"
)

target_link_libraries(${PROJECT_NAME} INTERFACE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/state_manager_coordinator.cpp
        public/state_manager/base_state_manager2.inl.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES
                public/state_manager/base_state_manager.h
                public/state_manager/base_state_manager.inl.cpp

                public/state_manager/base_state_manager2.h
                public/state_manager/state_manager.h
                public/state_manager/state_manager_coordinator.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_state_manager_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
