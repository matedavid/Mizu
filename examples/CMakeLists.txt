#
# MizuExample
#
add_executable(MizuExample example/main.cpp)
target_include_directories(MizuExample PRIVATE example/)

target_link_libraries(MizuExample PRIVATE MizuLib)

target_compile_definitions(MizuExample PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/example")

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

mizu_compile_shader(MizuPlasma ${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/TextureShader.vert ${PLASMA_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders")
mizu_compile_shader(MizuPlasma ${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/TextureShader.frag ${PLASMA_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders")

#[==[
mizu_compile_slang_shader(
	MizuPlasma 
	${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/TextureShader.slang 
	${PLASMA_EXAMPLE_SHADER_OUT_PATH}/TextureShader.vert.spv 
	vertex 
	vertexMain
)
mizu_compile_slang_shader(
	MizuPlasma 
	${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/TextureShader.slang 
	${PLASMA_EXAMPLE_SHADER_OUT_PATH}/TextureShader.frag.spv 
	fragment 
	fragmentMain
)
]==]
mizu_compile_slang_shader(
        MizuPlasma
        ${CMAKE_CURRENT_SOURCE_DIR}/plasma/shaders/PlasmaShader.comp.slang
        ${PLASMA_EXAMPLE_SHADER_OUT_PATH}/PlasmaShader.comp.spv
        compute
        computeMain
)

#
# MizuSkybox
#
add_executable(MizuSkybox skybox/main.cpp)
target_include_directories(MizuSkybox PRIVATE skybox/)

target_link_libraries(MizuSkybox PRIVATE MizuLib)

target_compile_definitions(MizuSkybox PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/skybox")

# Shaders
set(SKYBOX_EXAMPLE_SHADER_OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuSkybox PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${SKYBOX_EXAMPLE_SHADER_OUT_PATH}")

mizu_compile_shader(MizuSkybox ${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders/NormalShader.vert ${SKYBOX_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders")
mizu_compile_shader(MizuSkybox ${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders/NormalShader.frag ${SKYBOX_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders")
mizu_compile_shader(MizuSkybox ${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders/Skybox.vert ${SKYBOX_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders")
mizu_compile_shader(MizuSkybox ${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders/Skybox.frag ${SKYBOX_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/skybox/shaders")

#
# MizuMaterialShader
#
add_executable(MizuMaterialShader material_shader/main.cpp)
target_include_directories(MizuMaterialShader PRIVATE material_shader/)

target_link_libraries(MizuMaterialShader PRIVATE MizuLib)

target_compile_definitions(MizuMaterialShader PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/material_shader")

# Shaders
set(MATERIAL_SHADER_EXAMPLE_SHADER_OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_definitions(MizuMaterialShader PRIVATE MIZU_EXAMPLE_SHADERS_PATH="${MATERIAL_SHADER_EXAMPLE_SHADER_OUT_PATH}")

mizu_compile_shader(MizuMaterialShader ${CMAKE_CURRENT_SOURCE_DIR}/material_shader/shaders/AlbedoShader.vert ${MATERIAL_SHADER_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/material_shader/shaders")
mizu_compile_shader(MizuMaterialShader ${CMAKE_CURRENT_SOURCE_DIR}/material_shader/shaders/AlbedoShader.frag ${MATERIAL_SHADER_EXAMPLE_SHADER_OUT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/material_shader/shaders")
