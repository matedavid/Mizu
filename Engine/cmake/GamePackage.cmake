function(_mizu_parse_asset_mounts assets_root_list num_roots parsed_mounts_var)
    set(parsed_mounts)
    set(unprefixed_roots)

    foreach (root IN LISTS assets_root_list)
        if (root MATCHES "^([^:]+):(.+)$")
            # Has prefix like "shared:/path/to/assets"
            string(REGEX MATCH "^[^:]+" mount_name "${root}")
            string(REGEX MATCH ":.*" mount_path "${root}")
            string(SUBSTRING "${mount_path}" 1 -1 mount_path)  # Remove leading ':'
            cmake_path(ABSOLUTE_PATH mount_path NORMALIZE)
            list(APPEND parsed_mounts "${mount_name}|${mount_path}")
        else ()
            # No prefix
            list(APPEND unprefixed_roots "${root}")
        endif ()
    endforeach ()

    # Validation logic
    if (num_roots GREATER 1 AND unprefixed_roots)
        # Multiple roots but some unprefixed → error
        message(FATAL_ERROR
                "mizu_add_game_package: When specifying multiple ASSETS_ROOT, all must have a prefix. "
                "Example: mizu_add_game_package(... ASSETS_ROOT game:/path1 shared:/path2). "
                "Found unprefixed root(s). Only a single unprefixed root is allowed, which defaults to 'game'.")
    elseif (unprefixed_roots)
        # Single unprefixed root → default to "game"
        list(GET unprefixed_roots 0 root)
        cmake_path(ABSOLUTE_PATH root NORMALIZE)
        list(APPEND parsed_mounts "game|${root}")
    endif ()

    set(${parsed_mounts_var} "${parsed_mounts}" PARENT_SCOPE)
endfunction()

function(_mizu_generate_game_package_manifest target display_name parsed_mounts game_root)
    # Output file path: <target_output_dir>/{target_name}.manifest.package
    set(manifest_path "$<TARGET_FILE_DIR:${target}>/${target}.manifest.package")

    # Normalize game_root
    cmake_path(ABSOLUTE_PATH game_root NORMALIZE)

    # Build manifest content as a list of lines
    set(manifest_content)
    list(APPEND manifest_content "version=1")
    list(APPEND manifest_content "name=${target}")
    list(APPEND manifest_content "display_name=${display_name}")
    list(APPEND manifest_content "game_root=${game_root}")
    list(APPEND manifest_content "")

    foreach (mount IN LISTS parsed_mounts)
        list(APPEND manifest_content "asset_mount=${mount}")
    endforeach ()

    # Write manifest via generator expression
    # Convert list to string with newlines
    string(REPLACE ";" "\n" manifest_content_str "${manifest_content}")

    # Use file(GENERATE ...) to write at build time
    file(GENERATE
            OUTPUT "${manifest_path}"
            CONTENT "${manifest_content_str}\n"
    )

    # Store manifest path as target property
    # set_target_properties(${target} PROPERTIES MIZU_GAME_PACKAGE_MANIFEST_PATH "${manifest_path}")

    # Also set as a compile definition so the executable can find it
    # We use absolute path since the exe will be in the build output dir
    target_compile_definitions(${target} PUBLIC
            MIZU_PACKAGE_MANIFEST_PATH="$<TARGET_FILE_DIR:${target}>/${target}.manifest.package")
endfunction()

function(mizu_add_game_package)
    set(options)

    set(oneValueArgs
            TARGET
            DISPLAY_NAME
            GAME_ROOT
    )

    set(multiValueArgs
            ASSETS_ROOT
    )

    cmake_parse_arguments(MIZU
            "${options}"
            "${oneValueArgs}"
            "${multiValueArgs}"
            ${ARGN}
    )

    # Make sure all required arguments are provided
    if (NOT MIZU_TARGET)
        message(FATAL_ERROR "mizu_add_game_package: missing required argument TARGET")
    endif ()
    if (NOT MIZU_DISPLAY_NAME)
        message(FATAL_ERROR "mizu_add_game_package: missing required argument DISPLAY_NAME")
    endif ()
    if (NOT MIZU_GAME_ROOT)
        message(FATAL_ERROR "mizu_add_game_package: missing required argument GAME_ROOT")
    endif ()
    if (NOT MIZU_ASSETS_ROOT)
        message(FATAL_ERROR "mizu_add_game_package: missing required argument ASSETS_ROOT")
    endif ()

    # Parse and validate asset mounts
    list(LENGTH MIZU_ASSETS_ROOT num_roots)
    _mizu_parse_asset_mounts("${MIZU_ASSETS_ROOT}" "${num_roots}" parsed_mounts)

    # Create executable for target. Sources are intentionally added later by the caller.
    add_executable(${MIZU_TARGET})
    target_link_libraries(${MIZU_TARGET} PRIVATE MizuEngine)

    # Create package-owned shader pipeline executable.
    mizu_configure_shader_pipeline(${MIZU_TARGET}.Pipeline)

    # Set properties
    set_target_properties(${MIZU_TARGET} PROPERTIES MIZU_GAME_PACKAGE_DISPLAY_NAME "${MIZU_DISPLAY_NAME}")
    set_target_properties(${MIZU_TARGET} PROPERTIES MIZU_GAME_PACKAGE_GAME_ROOT "${MIZU_GAME_ROOT}")
    set_target_properties(${MIZU_TARGET} PROPERTIES MIZU_GAME_PACKAGE_PIPELINE_TARGET "${MIZU_TARGET}.Pipeline")

    # Generate manifest file and set compile definition
    _mizu_generate_game_package_manifest(${MIZU_TARGET} "${MIZU_DISPLAY_NAME}" "${parsed_mounts}" "${MIZU_GAME_ROOT}")

    message(STATUS "Configured Game Package:")
    message(STATUS "Target       : ${MIZU_TARGET}")
    message(STATUS "Display Name : ${MIZU_DISPLAY_NAME}")
    message(STATUS "Game Root    : ${MIZU_GAME_ROOT}")
    message(STATUS "Asset Mounts : ${parsed_mounts}")
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
