project(Example.Sandbox LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE MizuEngine)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")
