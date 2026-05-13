project(Engine.Asset LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_asset_module.h"
    EXPORT_MACRO_NAME "MIZU_ASSET_API"
)

mizu_configure_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/asset_registry.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES
                public/asset/asset.h
                public/asset/asset_handle.h
                public/asset/asset_registry.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_asset_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
