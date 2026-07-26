FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2
        GIT_TAG v3.15.2
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(catch2)

include(CTest)
include(Catch)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/unit_tests)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/render_tests)

