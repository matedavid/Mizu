project(Example.StreamingStressTest LANGUAGES CXX)

mizu_add_game_package(
	TARGET ${PROJECT_NAME}
	DISPLAY_NAME "Streaming Stress Test Example"
	GAME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
	ASSETS_ROOT
		"game:${CMAKE_CURRENT_SOURCE_DIR}"
		"shared:${MIZU_EXAMPLE_ASSETS_PATH}"
)

target_sources(${PROJECT_NAME} PRIVATE main.cpp)
