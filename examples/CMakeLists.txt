set(MIZU_EXAMPLE_ASSETS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets)

if(NOT EXISTS "${MIZU_EXAMPLE_ASSETS_PATH}/glTF-Sample-Assets/Models")
    message(STATUS "Downloading glTF-Sample-Assets submodule...")

    execute_process(
        COMMAND git submodule update --init --recursive --force
        WORKING_DIRECTORY ${MIZU_EXAMPLE_ASSETS_PATH}
        OUTPUT_VARIABLE result
    )

    if (NOT ${success} EQUAL 0)
        message(FATAL_ERROR "Failed to download glTF-Sample-Assets submodule. Please ensure you have git installed and try again.")
    endif()
endif()

#
# MizuPlasma
#
add_executable(MizuPlasma plasma/main.cpp)
target_include_directories(MizuPlasma PRIVATE plasma/)

target_link_libraries(MizuPlasma PRIVATE MizuLib)

target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/plasma")
target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

# Shaders
set(PLASMA_EXAMPLE_SHADER_OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${PLASMA_EXAMPLE_SHADER_OUT_PATH}")

mizu_compile_slang_shader(
        MizuPlasma
        ${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/TextureShader.slang
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}/TextureShader.vert.spv
        vertex
        vsMain 
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}
)
mizu_compile_slang_shader(
        MizuPlasma
        ${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/TextureShader.slang
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}/TextureShader.frag.spv
        fragment
        fsMain 
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}
)
mizu_compile_slang_shader(
        MizuPlasma
        ${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/PlasmaShader.comp.slang
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}/PlasmaShader.comp.spv
        compute
        csMain 
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}
)

#
# MizuDeferred
#
add_executable(MizuDeferred deferred/main.cpp)
target_include_directories(MizuDeferred PRIVATE deferred/)

target_link_libraries(MizuDeferred PRIVATE MizuLib)

target_compile_definitions(MizuDeferred PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/deferred")
target_compile_definitions(MizuDeferred PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")


#
# MizuSimpleRtx
#
add_executable(MizuSimpleRtx simple_rtx/main.cpp)
target_include_directories(MizuSimpleRtx PRIVATE simple_rtx/)

target_link_libraries(MizuSimpleRtx PRIVATE MizuLib)

target_compile_definitions(MizuSimpleRtx PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx")

set(SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuSimpleRtx PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}")

mizu_compile_slang_shader(
        MizuSimpleRtx
        ${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/shaders/Example.slang
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}/Example.raygen.spv
        raygen 
        rtxRaygen 
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}
)

mizu_compile_slang_shader(
        MizuSimpleRtx
        ${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/shaders/Example.slang
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}/Example.miss.spv
        miss 
        rtxMiss 
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}
)

mizu_compile_slang_shader(
        MizuSimpleRtx
        ${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/shaders/Example.slang
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}/Example.shadow.miss.spv
        miss 
        rtxShadowMiss 
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}
)

mizu_compile_slang_shader(
        MizuSimpleRtx
        ${CMAKE_CURRENT_SOURCE_DIR}/simple_rtx/shaders/Example.slang
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}/Example.closesthit.spv
        closestHit
        rtxClosestHit
        ${SIMPLE_RTX_EXAMPLE_SHADER_OUT_PATH}
)

#
# MizuStateManager
#
add_executable(MizuStateManager state_manager/main.cpp)
target_include_directories(MizuStateManager PRIVATE state_manager/)

target_link_libraries(MizuStateManager PRIVATE MizuLib)

target_compile_definitions(MizuStateManager PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/state_manager")
target_compile_definitions(MizuStateManager PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")

