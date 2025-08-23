#include "application/main_loop.h"

#include "base/debug/logging.h"
#include "base/threads/job_system.h"
#include <iostream>

using namespace Mizu;

/*
static void stupid_func(JobSystem& job_system)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::cout << std::this_thread::get_id() << "\n";

    const Job job = Job::create(&stupid_func, std::ref(job_system));
    job_system.schedule(job);
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

    JobSystemHandle job0 = job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 0"));
    JobSystemHandle job1 = job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 1"));
    JobSystemHandle job2 = job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 2"));
    JobSystemHandle job3 = job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 3"));

    job0.wait();
    job1.wait();
    job2.wait();
    job3.wait();

    MIZU_LOG_INFO("First 4 jobs finished, running the rest");

    JobSystemHandle job4 = job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 4"));
    JobSystemHandle job5 = job_system.schedule(Job::create(&stupid_func_prints_something, "Hello 5"));

    job4.wait();
    job5.wait();

    MIZU_LOG_INFO("All finished");

    /*
    MainLoop main_loop{};

    if (!main_loop.init())
        return 1;

    main_loop.run();
    */

    return 0;
}
