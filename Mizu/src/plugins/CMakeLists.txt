project(MizuPlugins)

add_library(${PROJECT_NAME} INTERFACE)

# CameraControllers
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/camera_controllers)
target_link_libraries(${PROJECT_NAME} INTERFACE MizuPlugin_CameraControllers)

# AssimpLoader
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/assimp_loader)
target_link_libraries(${PROJECT_NAME} INTERFACE MizuPlugin_AssimpLoader)
