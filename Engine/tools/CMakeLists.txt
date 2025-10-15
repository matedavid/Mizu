project(MizuTools C CXX)

add_executable(MizuShaderPipeline)
target_sources(MizuShaderPipeline PRIVATE 
	shader_pipeline/shader_pipeline.cpp
)

target_include_directories(MizuShaderPipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shader_pipeline/)
target_link_libraries(MizuShaderPipeline PRIVATE MizuLib)

target_compile_definitions(MizuShaderPipeline PRIVATE MIZU_ENGINE_SHADERS_SOURCE_PATH="${MIZU_ENGINE_SHADERS_SOURCE_PATH}")
target_compile_definitions(MizuShaderPipeline PRIVATE MIZU_ENGINE_SHADERS_OUTPUT_PATH="${MIZU_ENGINE_SHADERS_PATH}")

function(mizu_shader_pipeline_add_shader_provider shader_provider_path)
	target_sources(MizuShaderPipeline PRIVATE ${shader_provider_path})
endfunction()

function(mizu_shader_pipeline_add_shader_provider_define shader_provider_path define)
	mizu_shader_pipeline_add_shader_provider(${shader_provider_path})
	target_compile_definitions(MizuShaderPipeline PRIVATE ${define})
endfunction()

#
# Slang compiler
#

set(SLANG_VERSION 2025.18.2)

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

target_link_libraries(MizuShaderPipeline PRIVATE mizu_slang)


