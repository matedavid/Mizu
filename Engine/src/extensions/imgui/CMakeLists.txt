project(Engine.Extension.ImGui LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME "mizu_imgui_module.h"
    EXPORT_MACRO_NAME "MIZU_IMGUI_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_imgui_module)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Engine.Base
    Engine.Core
    Engine.Asset
    Engine.RenderCore
    Engine.Render
    Engine.StateManager
    Engine.Shader
)

# Pipeline
mizu_add_shader_module(Engine.Extension.ImGui.ShaderModule)
target_sources(Engine.Extension.ImGui.ShaderModule
    PRIVATE
        pipeline/imgui_shader_provider.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/pipeline/
            FILES
                pipeline/imgui.pipeline/imgui_shaders.h
)

target_compile_definitions(Engine.Extension.ImGui.ShaderModule PRIVATE MIZU_ENGINE_EXTENSION_IMGUI_PATH="${CMAKE_CURRENT_SOURCE_DIR}/pipeline")

target_link_libraries(${PROJECT_NAME} PUBLIC Engine.Extension.ImGui.ShaderModule)

# Dependencies
mizu_declare_dependency(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.93.0-docking
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(mizu_imgui SHARED
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp

        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(mizu_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(mizu_imgui PRIVATE glfw)

# ImGui has global state, so it needs to be a shared library. 
# That means overriding IMGUI_API on Windows, and the hidden visibility preset on Linux so that symbols are exported correctly.
if (WIN32)
    target_compile_definitions(mizu_imgui
            PRIVATE "IMGUI_API=__declspec(dllexport)"
            INTERFACE "IMGUI_API=__declspec(dllimport)"
    )
else ()
    set_target_properties(mizu_imgui PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF
    )
endif ()

target_link_libraries(${PROJECT_NAME} PUBLIC mizu_imgui)
