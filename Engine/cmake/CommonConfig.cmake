add_library(MizuProjectOptions INTERFACE)

if (MSVC)
    # /wd4715 removes the "not all paths return a value" warning
    # /wd4250 removes the "inherits x via dominance" warning
    # /wd4251 removes the "'type1' needs to have dll-interface to be used by clients of 'type2'" warning
    # /wd4275 removes the "non - DLL-interface class 'class_1' used as base for DLL-interface class 'class_2'" warning
    target_compile_options(MizuProjectOptions INTERFACE /W4 /WX /wd4715 /wd4250 /wd4251 /wd4275)
else ()
    target_compile_options(MizuProjectOptions INTERFACE -Wall -Wpedantic -Wextra -Wshadow -Wconversion -Werror)
endif ()

if (WIN32)
    target_compile_definitions(MizuProjectOptions INTERFACE MIZU_PLATFORM_WINDOWS)
    target_compile_definitions(MizuProjectOptions INTERFACE NOMINMAX)
    set(MIZU_PLATFORM_WINDOWS 1)
elseif (UNIX)
    target_compile_definitions(MizuProjectOptions INTERFACE MIZU_PLATFORM_UNIX)
    set(MIZU_PLATFORM_UNIX 1)
else ()
    message(FATAL_ERROR "Platform not recognized")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(MIZU_DEBUG 1)
endif ()

target_compile_definitions(MizuProjectOptions INTERFACE
        $<$<CONFIG:DEBUG>:MIZU_DEBUG>
        $<$<CONFIG:RELWITHDEBINFO>:MIZU_DEBUG>
)

