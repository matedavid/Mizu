project(MizuTests LANGUAGES C CXX)

add_executable(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Wshadow -Wconversion)

target_sources(${PROJECT_NAME} PRIVATE
        initialization.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE MizuLib)

#
# Dependencies
#
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(Catch2)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)