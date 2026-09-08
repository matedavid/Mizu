project(Test.UnitTests)

add_executable(${PROJECT_NAME})

mizu_configure_module(${PROJECT_NAME})

file(GLOB_RECURSE unit_test_sources LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

target_sources(${PROJECT_NAME} PRIVATE ${unit_test_sources})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Engine.Base 
    Engine.Core 
    Engine.StateManager
    Engine.Render
)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2WithMain)

catch_discover_tests(${PROJECT_NAME})
