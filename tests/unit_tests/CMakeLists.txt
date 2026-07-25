project(Test.UnitTests)

add_executable(${PROJECT_NAME})

mizu_configure_module(${PROJECT_NAME})

file(GLOB_RECURSE test_source_files LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

target_sources(${PROJECT_NAME} PRIVATE ${test_source_files})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Engine.Base 
    Engine.Core 
    Engine.StateManager
    Engine.Render
)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2WithMain)

catch_discover_tests(${PROJECT_NAME})
