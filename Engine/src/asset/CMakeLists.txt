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

if (NOT TARGET mizu_stb)
    add_library(mizu_stb STATIC)
    target_sources(mizu_stb PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image.cpp ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image_write.cpp)
    target_include_directories(mizu_stb PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb/)
    set_target_properties(mizu_stb PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

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
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)

target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Disable the -Wno-deprecated-copy error because of assimp
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated-copy -Wno-error=deprecated-copy)
endif ()

