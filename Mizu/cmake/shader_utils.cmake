#
# Slang compiler
#

set(SLANG_VERSION 2024.14.2)

function(download_slang_compiler git_tag os arch)
    set(url "https://github.com/shader-slang/slang/releases/download/v${git_tag}/slang-${git_tag}-${os}-${arch}.tar.gz")

    message(STATUS "[MIZU]: Downloading slang compiler from: " ${url})

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/slang/)
    file(DOWNLOAD ${url} ${CMAKE_CURRENT_BINARY_DIR}/slang/slangc_compiler.tar.gz)

    message(STATUS "[MIZU]: Finished downloading slang compiler")

    execute_process(
            COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_BINARY_DIR}/slang/slangc_compiler.tar.gz"
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/slang/
    )
endfunction()

find_program(SLANG_COMPILER NAMES "slangc" "slangc.exe" PATHS ${CMAKE_CURRENT_BINARY_DIR}/slang/bin/)

if (NOT SLANG_COMPILER)

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

    download_slang_compiler(${SLANG_VERSION} ${os} ${arch})

    find_program(SLANG_COMPILER_L NAMES "slangc" "slangc.exe" PATHS ${CMAKE_CURRENT_BINARY_DIR}/slang/bin/)
    message(STATUS ${SLANG_COMPILER_L})
    if (NOT SLANG_COMPILER_L)
        message(FATAL_ERROR "[MIZU]: Error downloading slang compiler, could not find executable")
    endif ()
    set(SLANG_COMPILER ${SLANG_COMPILER_L} PARENT_SCOPE)

endif ()

message(STATUS "[MIZU]: Found slang compiler: ${SLANG_COMPILER}")

#
# GLSL shaders
#

find_program(GLSLANG_COMPILER NAMES "glslang" "glslang.exe")
find_program(GLSLC_COMPILER NAMES "glslc" "glslc.exe")

#
# Utils
#

function(mizu_compile_glsl_shader target shader_path shader_output_path)
    if ((NOT GLSLANG_COMPILER) AND (NOT GLSLC_COMPILER))
        message(FATAL_ERROR "[MIZU]: No GLSL compiler found")
    endif ()

    if (GLSLANG_COMPILER)
        set(compile_command ${GLSLANG_COMPILER} ${shader_path} -o ${shader_output_path} -V)
    elseif (GLSLC_COMPILER)
        set(compile_command ${GLSLC_COMPILER} ${shader_path} -o ${shader_output_path})
    endif ()

    get_filename_component(shader_id ${shader_output_path} NAME)

    add_custom_command(
            OUTPUT ${shader_output_path}
            COMMAND ${compile_command}
            DEPENDS ${shader_path}
            COMMENT "Compiling shader ${shader_id}"
    )

    add_custom_target(${shader_id} DEPENDS ${shader_output_path})
    add_dependencies(${target} ${shader_id})
endfunction()

function(mizu_compile_slang_shader target shader_path shader_output_path stage entry)
    if (NOT SLANG_COMPILER)
        message(FATAL_ERROR "[MIZU]: slang compiler not found ${SLANG_COMPILER}")
    endif ()

    set(compile_command ${SLANG_COMPILER} -fvk-use-entrypoint-name ${shader_path} -o ${shader_output_path} -profile glsl_450 -target spirv -entry ${entry})

    get_filename_component(shader_id ${shader_output_path} NAME)

    message(STATUS ${out_command} " " ${target} " " ${shader_id})

    add_custom_command(
            OUTPUT ${shader_output_path}
            COMMAND ${compile_command}
            DEPENDS ${shader_path}
            COMMENT "Compiling shader ${shader_id}"
    )

    add_custom_target(${shader_id} DEPENDS ${shader_output_path})
    add_dependencies(${target} ${shader_id})
endfunction()

