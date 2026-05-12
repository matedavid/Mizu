function(mizu_add_game_package)
    set(options)

    set(oneValueArgs
            TARGET
            DISPLAY_NAME
            ASSETS_ROOT
    )

    set(multiValueArgs)

    cmake_parse_arguments(MIZU
            "${options}"
            "${oneValueArgs}"
            "${multiValueArgs}"
            ${ARGN}
    )

    # Make sure all required arguments are provided
    foreach (required_arg TARGET DISPLAY_NAME ASSETS_ROOT)
        if (NOT MIZU_${required_arg})
            message(FATAL_ERROR
                    "mizu_add_game_package: missing required argument ${required_arg}"
            )
        endif ()
    endforeach ()

    # Create executable for target. Sources are intentionally added later by the caller.
    add_executable(${MIZU_TARGET})
    target_link_libraries(${MIZU_TARGET} PRIVATE MizuEngine)

    # Create package-owned shader pipeline executable.
    mizu_configure_shader_pipeline(${MIZU_TARGET}.Pipeline)
    set_target_properties(${MIZU_TARGET} PROPERTIES MIZU_GAME_PACKAGE_PIPELINE_TARGET "${MIZU_TARGET}.Pipeline")

    message(STATUS "Configured Game Package:")
    message(STATUS "Target       : ${MIZU_TARGET}")
    message(STATUS "Display Name : ${MIZU_DISPLAY_NAME}")
    message(STATUS "Assets Root  : ${MIZU_ASSETS_ROOT}")
    message(STATUS "")

endfunction()

function(mizu_game_package_attach_shader_module package_target module_target)
    if (NOT TARGET ${package_target})
        message(FATAL_ERROR "mizu_game_package_attach_shader_module: unknown package target '${package_target}'")
    endif ()

    if (NOT TARGET ${module_target})
        message(FATAL_ERROR "mizu_game_package_attach_shader_module: unknown shader module target '${module_target}'")
    endif ()

    get_target_property(pipeline_target ${package_target} MIZU_GAME_PACKAGE_PIPELINE_TARGET)
    if (NOT pipeline_target OR pipeline_target STREQUAL "pipeline_target-NOTFOUND")
        message(FATAL_ERROR
            "mizu_game_package_attach_shader_module: target '${package_target}' is not configured as a game package")
    endif ()

    target_link_libraries(${package_target} PUBLIC $<LINK_LIBRARY:WHOLE_ARCHIVE,${module_target}>)
    mizu_attach_shader_module(${pipeline_target} ${module_target})
endfunction()
