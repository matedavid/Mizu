project(MizuExtension_ImGuiLayer)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE
    imgui_layer.cpp
    imgui_presenter.cpp

    imgui_vulkan_impl.cpp
)

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# ImGui
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/matedavid/imgui_cmake.git
        GIT_TAG docking
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui)

target_link_libraries(${PROJECT_NAME} INTERFACE imgui)
target_link_libraries(${PROJECT_NAME} INTERFACE imgui_impl_glfw)
target_link_libraries(${PROJECT_NAME} INTERFACE imgui_impl_vulkan)

