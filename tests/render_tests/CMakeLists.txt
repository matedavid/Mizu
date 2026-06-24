project(Test.RenderTests)

add_executable(${PROJECT_NAME})

mizu_configure_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    PRIVATE
        runner/render_tests_registry.cpp
        runner/render_tests_runner.cpp

    PRIVATE
        FILE_SET private_headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/
            FILES
                runner/render_tests_registry.h
)