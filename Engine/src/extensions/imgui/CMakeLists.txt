option(MIZU_IMGUI_ENABLED "Enable ImGui" ON)
target_compile_definitions(MizuEngineDefines INTERFACE MIZU_IMGUI_ENABLED=$<BOOL:${MIZU_IMGUI_ENABLED}>)

if (NOT MIZU_IMGUI_ENABLED)
    return()
endif ()

project(Engine.Extension.ImGui LANGUAGES CXX)

set(MIZU_IMGUI_PUBLIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/public)
set(MIZU_IMGUI_PRIVATE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/private)

#
# Engine.Extension.ImGui: context, panels and state manager, does not depend on Engine.Render 
#

add_library(Engine.Extension.ImGui SHARED)

generate_export_header(Engine.Extension.ImGui
    EXPORT_FILE_NAME "mizu_imgui_module.h"
    EXPORT_MACRO_NAME "MIZU_IMGUI_API"
)

mizu_configure_module(Engine.Extension.ImGui)

target_sources(Engine.Extension.ImGui
    PRIVATE
        private/imgui_context.cpp
        private/imgui_panels.cpp
        private/imgui_state_manager.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${MIZU_IMGUI_PUBLIC_DIR}
            FILES
                public/imgui/imgui_context.h
                public/imgui/imgui_panels.h
                public/imgui/imgui_state_manager.h
                public/imgui/imgui_types.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_imgui_module.h
)

target_link_libraries(Engine.Extension.ImGui PRIVATE
    Engine.Base
    Engine.Core
    Engine.Asset
    Engine.RenderCore
    Engine.StateManager
)

#
# Engine.Extension.ImGui.RenderModule: render module, depends on Engine.Render
#

add_library(Engine.Extension.ImGui.RenderModule SHARED)

generate_export_header(Engine.Extension.ImGui.RenderModule
    EXPORT_FILE_NAME "mizu_imgui_render_module.h"
    EXPORT_MACRO_NAME "MIZU_IMGUI_RENDER_API"
)

mizu_configure_module(Engine.Extension.ImGui.RenderModule)

target_sources(Engine.Extension.ImGui.RenderModule
    PRIVATE
        private/imgui_render_module.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${MIZU_IMGUI_PUBLIC_DIR}
            FILES
                public/imgui/imgui_render_module.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_imgui_render_module.h
)

target_link_libraries(Engine.Extension.ImGui.RenderModule PRIVATE
    Engine.Base
    Engine.Core
    Engine.Asset
    Engine.RenderCore
    Engine.Render
    Engine.StateManager
    Engine.Shader
)

target_link_libraries(Engine.Extension.ImGui.RenderModule PUBLIC Engine.Extension.ImGui)

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

target_link_libraries(Engine.Extension.ImGui.RenderModule PUBLIC Engine.Extension.ImGui.ShaderModule)

#
# Dependencies
#

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

target_link_libraries(Engine.Extension.ImGui PUBLIC mizu_imgui)
