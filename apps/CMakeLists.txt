## imgui
#FetchContent_Declare(
#        imgui
#        GIT_REPOSITORY https://github.com/matedavid/imgui_cmake.git
#        GIT_TAG docking
#        GIT_PROGRESS TRUE
#)
#FetchContent_MakeAvailable(imgui)
#
# #
# # MizuImGuiVulkan
# #
# add_executable(MizuImGuiVulkan imgui_vulkan/main.cpp)
# target_include_directories(MizuImGuiVulkan PRIVATE imgui_vulkan/)

# target_link_libraries(MizuImGuiVulkan PRIVATE MizuLib)

# target_link_libraries(MizuImGuiVulkan PRIVATE imgui)
# target_link_libraries(MizuImGuiVulkan PRIVATE imgui_impl_glfw)
# target_link_libraries(MizuImGuiVulkan PRIVATE imgui_impl_vulkan)


# #
# # MizuImGuiOpenGL
# #
# add_executable(MizuImGuiOpenGL imgui_opengl/main.cpp)
# target_include_directories(MizuImGuiOpenGL PRIVATE imgui_opengl/)

# target_link_libraries(MizuImGuiOpenGL PRIVATE MizuLib)

# target_link_libraries(MizuImGuiOpenGL PRIVATE imgui)
# target_link_libraries(MizuImGuiOpenGL PRIVATE imgui_impl_glfw)
# target_link_libraries(MizuImGuiOpenGL PRIVATE imgui_impl_opengl3)

#
# Example
#
add_executable(MizuExample example/main.cpp)
target_include_directories(MizuExample PRIVATE example/)

target_link_libraries(MizuExample PRIVATE MizuLib)