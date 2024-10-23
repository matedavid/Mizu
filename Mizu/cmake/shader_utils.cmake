#
# Slang compiler
#

function (download_slang_compiler git_tag os arch)
    set(url "https://github.com/shader-slang/slang/releases/download/v${git_tag}/slang-${git_tag}-${os}-${arch}.tar.gz")

    message(STATUS "Downloading slang compiler from: " ${url})

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/slang/)
    file(DOWNLOAD ${url} ${CMAKE_CURRENT_BINARY_DIR}/slang/slangc_compiler.tar.gz)

    message(STATUS "Finished downloading slang compiler")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_BINARY_DIR}/slang/slangc_compiler.tar.gz"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/slang/
    )
endfunction ()

find_program(SLANG_COMPILER NAMES "slangc" "slangc.exe" PATHS ${CMAKE_CURRENT_BINARY_DIR}/slang/bin/)

if (NOT SLANG_COMPILER)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(arch "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
        set(arch "aarch64")
    else()
        message(FATAL_ERROR "Unsupported architecture for slang binary releases: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(os "windows")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(os "macos")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(os "linux")
    else()
        message(FATAL_ERROR "Unsupported operating system for slang binary releases: ${CMAKE_SYSTEM_NAME}")
    endif()

    download_slang_compiler(2024.14.2 ${os} ${arch})

    find_program(SLANG_COMPILER_L NAMES "slangc" "slangc.exe" PATHS ${CMAKE_CURRENT_BINARY_DIR}/slang/bin/)
    set(SLANG_COMPILER ${SLANG_COMPILER_L})

endif ()

#
# Utils
#

find_program(GLSLANG_COMPILER NAMES "glslang" "glslang.exe")
find_program(GLSLC_COMPILER NAMES "glslc" "glslc.exe")

if ((NOT GLSLANG_COMPILER) AND (NOT GLSLC_COMPILER))
    message(FATAL_ERROR "No shader compilers found on system")
endif ()

function(mizu_compile_glsl_shader_command shader_path shader_output_path out_command)
    if (GLSLANG_COMPILER)
        set(${out_command} ${GLSLANG_COMPILER} ${shader_path} -o ${shader_output_path} -V PARENT_SCOPE)
    elseif (GLSLC_COMPILER)
        set(${out_command} ${GLSLC_COMPILER} ${shader_path} -o ${shader_output_path} PARENT_SCOPE)
    endif()
endfunction()
