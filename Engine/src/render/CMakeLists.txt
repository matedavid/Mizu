project(Engine.Render LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)

target_sources(${PROJECT_NAME}
    PRIVATE
        private/core/buffer_utils.cpp
        private/core/command_utils.cpp
        private/core/image_utils.cpp
        private/core/render_utils.cpp
        private/environment/environment.cpp
        private/material/material.cpp
        private/model/mesh.cpp
        private/render_graph/render_graph.cpp
        private/render_graph/render_graph_builder.cpp
        private/render_graph/render_graph_utils.cpp
        private/shader/shader_compiler.cpp
        private/shader/shader_manager.cpp
        private/shader/shader_reflection.cpp
        private/shader/shader_registry.cpp
        private/state_manager/camera_state_manager.cpp
        private/state_manager/imgui_state_manager.cpp
        private/state_manager/light_state_manager.cpp
        private/state_manager/renderer_settings_state_manager.cpp
        private/state_manager/state_manager_coordinator.cpp
        private/state_manager/static_mesh_state_manager.cpp
        private/state_manager/transform_state_manager.cpp
        private/systems/pipeline_cache.cpp
        private/systems/sampler_state_cache.cpp
        private/camera.cpp
        private/draw_block_manager.cpp
        private/renderer.cpp
        private/render_graph_renderer.cpp
        private/scene_renderer.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/private/
            FILES
                private/render_graph/render_graph_resource_aliasing.h

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/render/core/buffer_utils.h
                public/render/core/command_utils.h
                public/render/core/image_utils.h
                public/render/core/render_utils.h
                public/render/environment/environment.h
                public/render/material/material.h
                public/render/model/mesh.h
                public/render/render_graph/render_graph.h
                public/render/render_graph/render_graph_blackboard.h
                public/render/render_graph/render_graph_builder.h
                public/render/render_graph/render_graph_resources.h
                public/render/render_graph/render_graph_shader_parameters.h
                public/render/render_graph/render_graph_types.h
                public/render/render_graph/render_graph_utils.h
                public/render/shader/shader_compiler.h
                public/render/shader/shader_declaration.h
                public/render/shader/shader_manager.h
                public/render/shader/shader_permutation.h
                public/render/shader/shader_reflection.h
                public/render/shader/shader_registry.h
                public/render/state_manager/camera_state_manager.h
                public/render/state_manager/imgui_state_manager.h
                public/render/state_manager/light_state_manager.h
                public/render/state_manager/renderer_settings_state_manager.h
                public/render/state_manager/state_manager_coordinator.h
                public/render/state_manager/static_mesh_state_manager.h
                public/render/state_manager/transform_state_manager.h
                public/render/systems/pipeline_cache.h
                public/render/systems/sampler_state_cache.h
                public/render/camera.h
                public/render/draw_block_manager.h
                public/render/renderer.h
                public/render/render_graph_renderer.h
                public/render/render_graph_renderer_settings.h
                public/render/render_graph_renderer_shaders.h
                public/render/scene_renderer.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_render_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore.Dx12)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore.Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Core)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.StateManager)

target_compile_definitions(${PROJECT_NAME} PUBLIC MIZU_ENGINE_SHADERS_PATH="${MIZU_ENGINE_SHADERS_PATH}")

#
# Dependencies
#

# stb
add_library(mizu_stb STATIC)
target_sources(mizu_stb PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb/stb_image.cpp)
target_include_directories(mizu_stb PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb/)

target_link_libraries(${PROJECT_NAME} PRIVATE mizu_stb)

# slang
set(SLANG_VERSION 2025.19.1)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(arch "x86_64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(arch "aarch64")
else ()
    message(FATAL_ERROR "[MIZU]: Unsupported architecture for slang binary releases: ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(os "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(os "macos")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(os "linux")
else ()
    message(FATAL_ERROR "[MIZU]: Unsupported operating system for slang binary releases: ${CMAKE_SYSTEM_NAME}")
endif ()

set(slang_url "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${os}-${arch}.zip")

FetchContent_Declare(slang URL ${slang_url})
FetchContent_MakeAvailable(slang)
message(STATUS "[MIZU]: Finished downloading slang: ${slang_SOURCE_DIR}")

add_library(mizu_slang SHARED IMPORTED GLOBAL)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (MSVC)
		set_target_properties(mizu_slang
			PROPERTIES
				IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang.lib"
				IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang.dll"
		)
	else()
		message(FATAL_ERROR "Sorry, Slang does not provide precompiled binaries for MSYS/MinGW")
	endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set_target_properties(mizu_slang
		PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/libslang.so"
			IMPORTED_NO_SONAME TRUE
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set_target_properties(mizu_slang
		PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/libslang.dylib"
	)
endif()

target_include_directories(mizu_slang INTERFACE "${slang_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PUBLIC mizu_slang)

# json 
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
