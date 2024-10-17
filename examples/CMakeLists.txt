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

#
# MizuSkybox
#
add_executable(MizuSkybox skybox/main.cpp)
target_include_directories(MizuSkybox PRIVATE skybox/)

target_link_libraries(MizuSkybox PRIVATE MizuLib)

target_compile_definitions(MizuSkybox PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/skybox")

#
# MizuMaterialShader
#
add_executable(MizuMaterialShader material_shader/main.cpp)
target_include_directories(MizuMaterialShader PRIVATE material_shader/)

target_link_libraries(MizuMaterialShader PRIVATE MizuLib)

target_compile_definitions(MizuMaterialShader PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/material_shader")

