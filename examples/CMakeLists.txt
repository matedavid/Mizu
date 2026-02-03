FetchContent_Declare(
        gltf-sample-assets
        GIT_REPOSITORY https://github.com/KhronosGroup/glTF-Sample-Assets.git
        GIT_TAG main
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(gltf-sample-assets)

set(MIZU_EXAMPLE_ASSETS_PATH ${gltf-sample-assets_SOURCE_DIR})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/hello_triangle/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/plasma/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sandbox/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/)
