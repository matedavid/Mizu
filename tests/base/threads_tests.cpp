#include <catch2/catch_all.hpp>

#include "base/threads/job_system.h"

using namespace Mizu;

TEST_CASE("JobSystem initializes correctly", "[Base]")
{
    constexpr uint32_t NUM_THREADS = 4;
    constexpr size_t CAPACITY = 10;

    JobSystem job_system{NUM_THREADS, CAPACITY};
    job_system.init();

    uint32_t counter = 0;
    const Job job = Job::create([](uint32_t& value) { value += 1; }, std::ref(counter));

    const JobSystemHandle job0_handle = job_system.schedule(job);
    const JobSystemHandle job1_handle = job_system.schedule(job);

    job0_handle.wait();
    job1_handle.wait();

    REQUIRE(counter == 2);
}
