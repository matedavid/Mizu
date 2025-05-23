#
# MizuPlasma
#
add_executable(MizuPlasma plasma/main.cpp)
target_include_directories(MizuPlasma PRIVATE plasma/)

target_link_libraries(MizuPlasma PRIVATE MizuLib)

target_compile_definitions(MizuPlasma PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/plasma")

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
