FetchContent_Declare(
        gltf-sample-assets
        GIT_REPOSITORY https://github.com/KhronosGroup/glTF-Sample-Assets.git
        GIT_TAG main
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(gltf-sample-assets)

set(MIZU_EXAMPLE_ASSETS_PATH ${gltf-sample-assets_SOURCE_DIR})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/plasma/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sandbox/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/)

#
# MizuHelloTriangle
#
#[===[
add_executable(MizuHelloTriangle hello_triangle/main.cpp)
target_include_directories(MizuHelloTriangle PRIVATE hello_triangle/)

target_link_libraries(MizuHelloTriangle PRIVATE MizuEngine)

target_compile_definitions(MizuHelloTriangle PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/hello_triangle")
target_compile_definitions(MizuHelloTriangle PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

# Shaders
set(HELLO_TRIANGLE_EXAMPLE_SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuHelloTriangle PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${HELLO_TRIANGLE_EXAMPLE_SHADER_OUTPUT_PATH}")
#]===]

#[===[
mizu_shader_pipeline_add_shader_provider_define(
    hello_triangle/hello_triangle_shader_provider.cpp 
    MIZU_EXAMPLE_HELLO_TRIANGLE_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/hello_triangle"
    MIZU_EXAMPLE_HELLO_TRIANGLE_SHADERS_OUTPUT_PATH="${HELLO_TRIANGLE_EXAMPLE_SHADER_OUTPUT_PATH}"
)
#]===]
