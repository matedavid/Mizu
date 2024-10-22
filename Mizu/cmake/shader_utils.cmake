find_program(GLSLANG_COMPILER NAMES "glslang" "glslang.exe")
find_program(GLSLC_COMPILER NAMES "glslc" "glslc.exe")

if ((NOT GLSLANG_COMPILER) AND (NOT GLSLC_COMPILER) AND (NOT DXC_COMPILER))
    message(FATAL_ERROR "No shader compilers found on system")
endif ()

function(mizu_compile_glsl_shader_command shader_path shader_output_path out_command)
    if (GLSLANG_COMPILER)
        set(${out_command} ${GLSLANG_COMPILER} ${shader_path} -o ${shader_output_path} -V PARENT_SCOPE)
    elseif (GLSLC_COMPILER)
        set(${out_command} ${GLSLC_COMPILER} ${shader_path} -o ${shader_output_path} PARENT_SCOPE)
    endif()
endfunction()
