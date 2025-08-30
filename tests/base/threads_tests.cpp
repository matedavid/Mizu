#include <catch2/catch_all.hpp>

#include "base/threads/job_system.h"

using namespace Mizu;

static constexpr uint32_t BASE_NUM_THREADS = 4;
static constexpr size_t BASE_CAPACITY = 10;

TEST_CASE("JobSystem initializes correctly", "[Base]")
{
    JobSystem job_system{BASE_NUM_THREADS, BASE_CAPACITY};
    job_system.init();

    uint32_t counter = 0;
    const Job job = Job::create([](uint32_t& value) { value += 1; }, std::ref(counter));

    const JobSystemHandle job0_handle = job_system.schedule(job);
    const JobSystemHandle job1_handle = job_system.schedule(job);

    job0_handle.wait();
    job1_handle.wait();

    REQUIRE(counter == 2);
}

TEST_CASE("JobSystem schedules jobs correctly", "[Base]")
{
    JobSystem job_system{BASE_NUM_THREADS, BASE_CAPACITY};
    job_system.init();

    SECTION("Single job dependencies work as expected")
    {
        constexpr uint32_t NUM_TEST_VALUES = 5;

        std::array<bool, NUM_TEST_VALUES> dependencies{};
        std::array<JobSystemHandle, NUM_TEST_VALUES> job_handles;

        const auto job_func = [](uint32_t i, std::array<bool, NUM_TEST_VALUES>& dependencies) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            dependencies[i] = true;

            if (i == 0)
                return;

            REQUIRE(dependencies[i - 1]);
        };

        for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
        {
            Job job = Job::create(job_func, i, std::ref(dependencies));
            if (i != 0)
                job.depends_on(job_handles[i - 1]);

            job_handles[i] = job_system.schedule(job);
        }

        job_handles[job_handles.size() - 1].wait();

        for (bool value : dependencies)
        {
            REQUIRE(value);
        }
    }

    SECTION("Multiple job dependencies work as expected")
    {
        constexpr uint32_t NUM_DEPENDENCIES = 5;

        std::array<bool, NUM_DEPENDENCIES> dependencies{};

        const auto job_func = [](uint32_t i, std::array<bool, NUM_DEPENDENCIES>& dependencies) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            dependencies[i] = true;
        };

        Job final_job = Job::create(
            [&](const std::array<bool, NUM_DEPENDENCIES>& dependencies_vec) {
                for (uint32_t i = 0; i < NUM_DEPENDENCIES; ++i)
                {
                    REQUIRE(dependencies_vec[i]);
                }
            },
            std::ref(dependencies));

        for (uint32_t i = 0; i < NUM_DEPENDENCIES; ++i)
        {
            const Job job = Job::create(job_func, i, std::ref(dependencies));
            const JobSystemHandle job_handle = job_system.schedule(job);

            final_job.depends_on(job_handle);
        }

        const JobSystemHandle final_job_handle = job_system.schedule(final_job);
        final_job_handle.wait();
    }

    SECTION("Scheduling multiple jobs works as expected")
    {
        constexpr uint32_t NUM_TEST_VALUES = 10;

        std::array<uint32_t, NUM_TEST_VALUES> results{};
        std::array<Job, NUM_TEST_VALUES> jobs{};

        const auto job_func = [](uint32_t x, std::array<uint32_t, NUM_TEST_VALUES>& results) {
            results[x] = x * x;
        };

        for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
        {
            jobs[i] = Job::create(job_func, i, std::ref(results));
        }

        const JobSystemHandle job_handle = job_system.schedule(jobs);
        job_handle.wait();

        for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
        {
            const uint32_t expected = i * i;
            REQUIRE(results[i] == expected);
        }
    }

    SECTION("Job with affinity runs on the appropriate worker")
    {
        std::array<std::thread::id, 3> thread_ids;

        constexpr ThreadAffinity Affinity_1 = 1u;
        constexpr ThreadAffinity Affinity_2 = 2u;

        constexpr uint32_t NUM_TEST_VALUES = 5;

        const auto job_func = [](ThreadAffinity affinity, bool first, std::array<std::thread::id, 3>& threads) {
            if (first)
                threads[affinity] = std::this_thread::get_id();
            else
                REQUIRE(threads[affinity] == std::this_thread::get_id());
        };

        const Job job1_first = Job::create(job_func, Affinity_1, true, std::ref(thread_ids)).set_affinity(Affinity_1);
        const Job job2_first = Job::create(job_func, Affinity_2, true, std::ref(thread_ids)).set_affinity(Affinity_2);

        job_system.schedule(job1_first);
        job_system.schedule(job2_first);

        const Job job1_second = Job::create(job_func, Affinity_1, false, std::ref(thread_ids)).set_affinity(Affinity_1);
        const Job job2_second = Job::create(job_func, Affinity_2, false, std::ref(thread_ids)).set_affinity(Affinity_2);

        for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
        {
            const JobSystemHandle handle0 = job_system.schedule(job1_second);
            const JobSystemHandle handle1 = job_system.schedule(job2_second);

            handle0.wait();
            handle1.wait();
        }
    }
}