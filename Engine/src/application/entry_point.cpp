#include "application/main_loop.h"

#include "base/threads/job_system.h"

#include "base/debug/logging.h"

#include <iostream>

using namespace Mizu;

/*
static void stupid_func(JobSystem* job_system, ThreadAffinity affinity)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::cout << std::this_thread::get_id() << "\n";

    const Job job = Job::create(&stupid_func, job_system, affinity).set_thread_affinity(affinity);
    job_system->execute(job);
}
*/

static void stupid_func_prints_something(const char* something)
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    uint64_t id = std::stoull(ss.str());

    MIZU_LOG_INFO("{} from {}", something, id);
}

int main()
{
    MIZU_LOG_SETUP;

    JobSystem job_system{std::thread::hardware_concurrency(), 120};
    job_system.init();

    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 0"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 1"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 2"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 3"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 4"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 5"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 6"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 7"));

    job_system.wait_all_jobs_finished();

    MIZU_LOG_INFO("Waited for all jobs to finish...");

    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 10"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 11"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 12"));
    job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 13"));

    job_system.wait_all_jobs_finished();

    /*
    MainLoop main_loop{};

    if (!main_loop.init())
        return 1;

    main_loop.run();
    */

    return 0;
}
