project(MizuExtensions)

add_library(${PROJECT_NAME} INTERFACE)

# AssimpLoader
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/assimp_loader)
target_link_libraries(${PROJECT_NAME} INTERFACE MizuExtension_AssimpLoader)

# CameraControllers
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/camera_controllers)
target_link_libraries(${PROJECT_NAME} INTERFACE MizuExtension_CameraControllers)

# ImGuiLayer
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/imgui)
# target_link_libraries(${PROJECT_NAME} INTERFACE MizuExtension_ImGui)

