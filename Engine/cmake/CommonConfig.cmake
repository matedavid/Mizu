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
    $<$<CONFIG:DEBUG>:MIZU_DEBUG>
    $<$<CONFIG:RELWITHDEBINFO>:MIZU_DEBUG>
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

