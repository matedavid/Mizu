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

    message(STATUS "Configured Game Package:")
    message(STATUS "Target       : ${MIZU_TARGET}")
    message(STATUS "Display Name : ${MIZU_DISPLAY_NAME}")
    message(STATUS "Assets Root  : ${MIZU_ASSETS_ROOT}")
    message(STATUS "")

endfunction()
