#
# MizuExample
#
add_executable(MizuExample example/main.cpp)
target_include_directories(MizuExample PRIVATE example/)

target_link_libraries(MizuExample PRIVATE MizuLib)

target_compile_definitions(MizuExample PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/example")
