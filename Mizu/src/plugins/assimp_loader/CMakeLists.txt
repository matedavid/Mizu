project(MizuPlugin_AssimpLoader)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE
	assimp_loader.cpp
)

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(assimp)

target_link_libraries(${PROJECT_NAME} INTERFACE assimp)
