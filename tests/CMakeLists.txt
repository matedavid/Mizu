project(MizuTests)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
        base/threads_tests.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC MizuLib)

FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2
        GIT_TAG v3.10.0
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(catch2)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2WithMain)
