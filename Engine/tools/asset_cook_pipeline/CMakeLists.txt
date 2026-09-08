project(Tools.AssetCookPipeline LANGUAGES CXX)

add_library(${PROJECT_NAME} OBJECT)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine.Base
    Engine.Asset
    Engine.Package
    Engine.RenderCore
    Engine.Core
    Engine.Shader
)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_ENGINE_SHADERS_PATH="${MIZU_ENGINE_SHADERS_PATH}")

set(MIZU_PIPELINE_ENTRY_POINT "${CMAKE_CURRENT_SOURCE_DIR}/entry/pipeline_main.cpp" CACHE INTERNAL "Asset cook pipeline entry point source")
set(MIZU_PIPELINE_PRIVATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private" CACHE INTERNAL "Asset cook pipeline private include directory")

function(mizu_configure_asset_cook_pipeline target_name)
    target_sources(${target_name} PRIVATE $<TARGET_OBJECTS:Tools.AssetCookPipeline>)
    target_include_directories(${target_name} PRIVATE ${MIZU_PIPELINE_PRIVATE_DIR})

    target_link_libraries(${target_name} PRIVATE
        Engine.Base
        Engine.Asset
        Engine.Package
        Engine.RenderCore
        Engine.Core
        Engine.Shader
    )

    target_link_libraries(${target_name} PRIVATE assimp)
    target_link_libraries(${target_name} PRIVATE mizu_stb)

    target_link_libraries(${target_name} PRIVATE MizuEngineDefines MizuPrivateOptions)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})
endfunction()

function(mizu_add_shader_module target_name)
    add_library(${target_name} STATIC)

    target_link_libraries(${target_name} PRIVATE MizuEngineDefines MizuPrivateOptions Engine.Base Engine.RenderCore Engine.Shader)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD} POSITION_INDEPENDENT_CODE ON)
endfunction()

function(mizu_attach_shader_module pipeline_target module_target)
    target_link_libraries(${pipeline_target} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${module_target}>)
endfunction()

#
# Dependencies
#

# Assimp
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.5
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_DRACO ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DRACO_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)

# Workaround: assimp's contrib/draco/CMakeLists.txt writes the generated draco_features.h to
# ${CMAKE_BINARY_DIR} (the *top-level* project's build dir) instead of assimp's own binary dir,
# so when assimp is pulled in via FetchContent from a parent project (as here), the assimp target's
# own include paths never cover the directory the header actually lands in. Add it explicitly.
if (TARGET assimp)
    target_include_directories(assimp PRIVATE ${CMAKE_BINARY_DIR})
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Disable the -Wno-deprecated-copy error because of assimp
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated-copy -Wno-error=deprecated-copy)
endif ()

# Stb
add_library(mizu_stb STATIC)
target_sources(mizu_stb PRIVATE
        ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image.cpp
        ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image_write.cpp
        ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image_resize2.cpp
)

target_include_directories(mizu_stb PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb/)
set_target_properties(mizu_stb PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

