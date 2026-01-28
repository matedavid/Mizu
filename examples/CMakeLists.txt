FetchContent_Declare(
        gltf-sample-assets
        GIT_REPOSITORY https://github.com/KhronosGroup/glTF-Sample-Assets.git
        GIT_TAG main
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(gltf-sample-assets)

set(MIZU_EXAMPLE_ASSETS_PATH ${gltf-sample-assets_SOURCE_DIR})

#
# MizuPlasma
#
#[===[
add_executable(MizuPlasma plasma/main.cpp)
target_include_directories(MizuPlasma PRIVATE plasma/)

target_link_libraries(MizuPlasma PRIVATE MizuEngine)

target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/plasma")
target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

# Shaders
set(PLASMA_EXAMPLE_SHADER_OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${PLASMA_EXAMPLE_SHADER_OUT_PATH}")
#]===]

#[===[
mizu_shader_pipeline_add_shader_provider_define(
    plasma/plasma_shader_provider.cpp 
    MIZU_EXAMPLE_PLASMA_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders"
    MIZU_EXAMPLE_PLASMA_SHADERS_OUTPUT_PATH="${PLASMA_EXAMPLE_SHADER_OUT_PATH}"
)
#]===]

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

#
# MizuSimpleRtx
#
#[===[
add_executable(MizuSimpleRtx simple_rtx/main.cpp)
target_include_directories(MizuSimpleRtx PRIVATE simple_rtx/)

target_link_libraries(MizuSimpleRtx PRIVATE MizuEngine)

target_compile_definitions(MizuSimpleRtx PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx")

set(SIMPLE_RTX_EXAMPLE_SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuSimpleRtx PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${SIMPLE_RTX_EXAMPLE_SHADER_OUTPUT_PATH}")

mizu_shader_pipeline_add_shader_provider_define(
    simple_rtx/simple_rtx_shader_provider.cpp 
    MIZU_EXAMPLE_SIMPLE_RTX_SHADERS_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/shaders"
    MIZU_EXAMPLE_SIMPLE_RTX_SHADERS_OUTPUT_PATH="${SIMPLE_RTX_EXAMPLE_SHADER_OUTPUT_PATH}"
)
]===]

#
# MizuStateManager
#
add_executable(MizuStateManager state_manager/main.cpp)
target_include_directories(MizuStateManager PRIVATE state_manager/)

target_link_libraries(MizuStateManager PRIVATE MizuEngine)

target_compile_definitions(MizuStateManager PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/state_manager")
target_compile_definitions(MizuStateManager PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

