project(Engine.StateManager LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE MizuProjectOptions)

target_sources(${PROJECT_NAME}
    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/state_manager/base_state_manager.h
                public/state_manager/base_state_manager.inl.cpp
                public/state_manager/state_manager.h
)

target_link_libraries(${PROJECT_NAME} INTERFACE Engine.Base)
