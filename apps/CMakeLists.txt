#
# MizuImGuiVulkan
#
add_executable(MizuImGuiVulkan imgui_vulkan/main.cpp)

target_include_directories(MizuImGuiVulkan PRIVATE imgui_vulkan/)

# imgui
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/matedavid/imgui_cmake.git
        GIT_TAG docking 
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)

target_link_libraries(MizuImGuiVulkan PRIVATE MizuLib)

target_link_libraries(MizuImGuiVulkan PRIVATE imgui)
target_link_libraries(MizuImGuiVulkan PRIVATE imgui_impl_glfw)
target_link_libraries(MizuImGuiVulkan PRIVATE imgui_impl_vulkan)
