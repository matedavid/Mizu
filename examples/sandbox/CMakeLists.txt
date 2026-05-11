project(Example.Sandbox LANGUAGES CXX)

mizu_add_game_package(
	TARGET ${PROJECT_NAME}
	DISPLAY_NAME "Sandbox Example"
	ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(${PROJECT_NAME} PRIVATE main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_EXAMPLE_ASSETS_PATH="${MIZU_EXAMPLE_ASSETS_PATH}")
