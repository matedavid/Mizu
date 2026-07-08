add_library(MizuEngineDefines INTERFACE)
add_library(MizuPrivateOptions INTERFACE)

if (MSVC)
    # /wd4715 removes the "not all paths return a value" warning
    # /wd4250 removes the "inherits x via dominance" warning
    # /wd4251 removes the "'type1' needs to have dll-interface to be used by clients of 'type2'" warning
    # /wd4275 removes the "non - DLL-interface class 'class_1' used as base for DLL-interface class 'class_2'" warning
    target_compile_options(MizuPrivateOptions INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/WX>
        $<$<COMPILE_LANGUAGE:CXX>:/wd4715>
        $<$<COMPILE_LANGUAGE:CXX>:/wd4250>
        $<$<COMPILE_LANGUAGE:CXX>:/wd4251>
        $<$<COMPILE_LANGUAGE:CXX>:/wd4275>
    )
else ()
    target_compile_options(MizuPrivateOptions INTERFACE 
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wshadow>
        $<$<COMPILE_LANGUAGE:CXX>:-Wconversion>
        $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif ()

if (WIN32)
    target_compile_definitions(MizuEngineDefines INTERFACE MIZU_PLATFORM_WINDOWS=1)
    target_compile_definitions(MizuPrivateOptions INTERFACE NOMINMAX)
    set(MIZU_PLATFORM_WINDOWS 1)
elseif (UNIX)
    target_compile_definitions(MizuEngineDefines INTERFACE MIZU_PLATFORM_UNIX=1)
    set(MIZU_PLATFORM_UNIX 1)
else ()
    message(FATAL_ERROR "Platform not recognized")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(MIZU_DEBUG 1)
else ()
    set(MIZU_DEBUG 0)
endif ()

target_compile_definitions(MizuEngineDefines INTERFACE
    MIZU_DEBUG=$<IF:$<OR:$<CONFIG:DEBUG>,$<CONFIG:RELWITHDEBINFO>>,1,0>
)

function (mizu_configure_module module_name)
    target_link_libraries(${module_name}
        PUBLIC MizuEngineDefines
        PRIVATE MizuPrivateOptions
    )

    set_target_properties(${module_name} PROPERTIES 
        UNITY_BUILD ${MIZU_USE_UNITY_BUILD}
    )
endfunction ()

function (mizu_set_module_sources module_name export_file_name)
    cmake_path(APPEND module_private_source_dir ${CMAKE_CURRENT_SOURCE_DIR} "private")
    cmake_path(APPEND module_public_source_dir  ${CMAKE_CURRENT_SOURCE_DIR} "public")

    file(GLOB_RECURSE private_cpp_files LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${module_private_source_dir}/*.cpp")
    file(GLOB_RECURSE private_h_files   LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${module_private_source_dir}/*.h" "${module_private_source_dir}/*.inl.cpp")
    file(GLOB_RECURSE public_h_files    LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "${module_public_source_dir}/*.h"  "${module_public_source_dir}/*.inl.cpp")

    target_sources(${module_name}
        PRIVATE
            ${private_cpp_files}

        PRIVATE
            FILE_SET private_headers
                TYPE HEADERS
                BASE_DIRS ${module_private_source_dir}
                FILES ${private_h_files}

        PUBLIC
            FILE_SET public_headers
                TYPE HEADERS
                BASE_DIRS ${module_public_source_dir}
                FILES ${public_h_files}

            FILE_SET generated_headers
                TYPE HEADERS
                BASE_DIRS $<TARGET_PROPERTY:${module_name},BINARY_DIR>
                FILES ${CMAKE_CURRENT_BINARY_DIR}/${export_file_name}.h
    )
endfunction ()

