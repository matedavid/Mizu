project(Test.UnitTests)

add_executable(${PROJECT_NAME})

mizu_configure_module(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
        base/containers_tests.cpp

        core/job_system/fibers_tests.cpp
        core/job_system/intrusive_free_list_tests.cpp
        core/job_system/job_system_tests.cpp
        core/job_system/job_system_stress_tests.cpp
        core/job_system/mpsc_queue_tests.cpp
        core/job_system/work_stealing_deque_tests.cpp

        state_manager/base_state_manager_tests.cpp
        state_manager/state_manager_coordinator_tests.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Engine.Base 
    Engine.Core 
    Engine.StateManager
)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2WithMain)

catch_discover_tests(${PROJECT_NAME})
