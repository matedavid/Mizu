project(Engine.Extension.AssimpLoader LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_assimp_loader_module.h"
    EXPORT_MACRO_NAME "MIZU_ASSIMP_LOADER_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Disable the -Wno-deprecated-copy error
    target_compile_options(${PROJECT_NAME} INTERFACE -Wno-deprecated-copy -Wno-error=deprecated-copy)
endif ()

target_sources(${PROJECT_NAME}
    PRIVATE
        private/assimp_loader.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/assimp_loader/assimp_loader.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_assimp_loader_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base Engine.RenderCore  Engine.Shader Engine.Render)

#
# Dependencies
#

# Assimp
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)

target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
