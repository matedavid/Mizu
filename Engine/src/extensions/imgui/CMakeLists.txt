# Based on: https://github.com/tamaskenez/allthesmallthings/blob/master/s/imgui/CMakeLists.txt

project(MizuExtension_ImGui)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE
    imgui_presenter.cpp

    imgui_vulkan_impl.cpp
)

target_link_libraries(${PROJECT_NAME} INTERFACE imgui)
target_link_libraries(${PROJECT_NAME} INTERFACE imgui_impl_glfw)
target_link_libraries(${PROJECT_NAME} INTERFACE imgui_impl_vulkan)

# ImGui
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b-docking
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui)

set(ROOT ${imgui_SOURCE_DIR})

add_library(imgui STATIC
        ${ROOT}/imgui.cpp
        ${ROOT}/imgui.h
        ${ROOT}/imstb_rectpack.h
        ${ROOT}/imstb_textedit.h
        ${ROOT}/imstb_truetype.h
        ${ROOT}/imgui_demo.cpp
        ${ROOT}/imgui_draw.cpp
        ${ROOT}/imgui_internal.h
        ${ROOT}/imgui_tables.cpp
        ${ROOT}/imgui_widgets.cpp
        ${ROOT}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui PUBLIC
        $<BUILD_INTERFACE:${ROOT}>
        $<INSTALL_INTERFACE:include>)

function(create_backend BACKEND)
    set(NAME imgui_impl_${BACKEND})
    set(HEADER ${ROOT}/backends/${NAME}.h)

    add_library(${NAME} STATIC ${ROOT}/backends/${NAME}.cpp ${HEADER})

    target_link_libraries(${NAME} PUBLIC imgui)
    target_include_directories(${NAME} PUBLIC
            $<BUILD_INTERFACE:${ROOT}/backends>
            $<INSTALL_INTERFACE:include>)

    LIST(APPEND INSTALL_TARGETS ${NAME})
    LIST(APPEND INSTALL_HEADERS ${HEADER})
endfunction()

create_backend(glfw)

target_link_libraries(imgui_impl_glfw PRIVATE glfw)
    
create_backend(vulkan)
    
find_package(Vulkan REQUIRED)
target_link_libraries(imgui_impl_vulkan PRIVATE ${Vulkan_LIBRARIES})
target_include_directories(imgui_impl_vulkan PUBLIC ${Vulkan_INCLUDE_DIRS})


