#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <vector>

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/device.h"

#include "runner/render_tests_registry.h"
#include "runner/render_tests_runner.h"

using namespace Mizu;

#ifndef MIZU_RENDER_TESTS_REFERENCE_IMAGES_PATH
#error "The reference images path has not been defined"
#endif

static constexpr size_t MAX_TOTAL_RENDER_TESTS = 2048;

static ExecutionType parse_execution_type_string(const char* str)
{
    if (strcmp(str, "update_images") == 0)
    {
        return ExecutionType::UpdateReferenceImages;
    }

    if (strcmp(str, "compare_images") == 0)
    {
        return ExecutionType::CompareImages;
    }

    MIZU_LOG_ERROR("Invalid execution type string: {}", str);
    return ExecutionType::CompareImages;
}

static void print_results(const RenderTestsRunnerResults& results, const RenderTestEnvironment& environment)
{
    if (results.failed_tests > 0)
    {
        MIZU_LOG_ERROR("Render Tests Results for {}:", graphics_api_to_string(environment.graphics_api));
        MIZU_LOG_ERROR("  Passed tests: {}", results.passed_tests);
        MIZU_LOG_ERROR("  Failed tests: {}", results.failed_tests);
    }
    else
    {
        MIZU_LOG_INFO("Render Tests Results for {}:", graphics_api_to_string(environment.graphics_api));
        MIZU_LOG_INFO("  Passed tests: {}", results.passed_tests);
        MIZU_LOG_INFO("  Failed tests: {}", results.failed_tests);
    }
}

int main(int32_t argc, const char* argv[])
{
    ExecutionType execution_type = ExecutionType::CompareImages;
    if (argc >= 2)
    {
        execution_type = parse_execution_type_string(argv[1]);
    }

    std::vector<RenderTest*> render_tests{};
    render_tests.resize(MAX_TOTAL_RENDER_TESTS);

    RenderTestsRegistry& registry = RenderTestsRegistry::get();
    const std::span<RenderTest*> registered_render_tests = registry.get_render_tests();

    inplace_vector<RenderTestsInfo, 10> render_tests_info = {
#if MIZU_RENDER_CORE_VULKAN_ENABLED
        RenderTestsInfo{
            .environment = {.graphics_api = GraphicsApi::Vulkan},
        },
#endif
#if MIZU_RENDER_CORE_DX12_ENABLED
        RenderTestsInfo{
            .environment = {.graphics_api = GraphicsApi::Dx12},
        },
#endif
    };

    uint32_t test_num = 0;
    for (RenderTestsInfo& test_info : render_tests_info)
    {
        const uint32_t tests_offset = test_num;

        for (RenderTest* test : registered_render_tests)
        {
            if (test->should_run_test(test_info.environment))
            {
                render_tests[test_num++] = test;
            }
        }

        test_info.render_tests = std::span<RenderTest*>(render_tests.data() + tests_offset, test_num - tests_offset);
        test_info.execution_type = execution_type;
    }

    if (test_num == 0)
    {
        MIZU_LOG_INFO("No tests to run, exiting");
        return 0;
    }

    const std::filesystem::path temp_directory = std::filesystem::temp_directory_path();
    const std::filesystem::path session_directory =
        temp_directory / std::format("Mizu_RenderTestsSession_{}", std::time(nullptr));

    for (RenderTestsInfo& test_info : render_tests_info)
    {
        test_info.session_path = session_directory;
        test_info.reference_images_path = std::filesystem::path{MIZU_RENDER_TESTS_REFERENCE_IMAGES_PATH};
    }

    for (const RenderTestsInfo& tests_info : render_tests_info)
    {
        RenderTestsRunner runner{tests_info};
        runner.run_tests();

        if (execution_type == ExecutionType::CompareImages)
        {
            print_results(runner.get_results(), tests_info.environment);
        }
    }

    if (execution_type == ExecutionType::CompareImages)
    {
        MIZU_LOG_INFO("Render test session results stored at: {}", session_directory.string());
    }

    return 0;
}
