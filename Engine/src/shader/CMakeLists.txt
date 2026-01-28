project(Engine.Shader LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_shader_module.h"
    EXPORT_MACRO_NAME "MIZU_SHADER_API"
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuProjectOptions)
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})

target_sources(${PROJECT_NAME}
    PRIVATE
        private/shader_compiler.cpp
        private/shader_reflection.cpp
        private/shader_registry.cpp

    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public/
            FILES 
                public/shader/shader_compiler.h
                public/shader/shader_declaration.h
                public/shader/shader_permutation.h
                public/shader/shader_reflection.h
                public/shader/shader_registry.h

        FILE_SET generated_headers
            TYPE HEADERS
            BASE_DIRS $<TARGET_PROPERTY:${PROJECT_NAME},BINARY_DIR>
            FILES ${CMAKE_CURRENT_BINARY_DIR}/mizu_shader_module.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCore)

#
# Dependencies
#

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
