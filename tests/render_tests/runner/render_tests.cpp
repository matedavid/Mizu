#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <vector>

#include "base/containers/inplace_vector.h"
#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"

#include "runner/render_tests_runner.h"

using namespace Mizu;

#ifndef MIZU_RENDER_TESTS_REFERENCE_IMAGES_PATH
#error "The reference images path has not been defined"
#endif

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

static void print_results(
    [[maybe_unused]] const RenderTestsRunnerResults& results,
    [[maybe_unused]] const RenderTestEnvironment& environment)
{
    if (results.failed_tests > 0)
    {
        MIZU_LOG_ERROR("Render Tests Results for {}:", meta::enum_name(environment.graphics_api));
        MIZU_LOG_ERROR("  Passed tests: {}", results.passed_tests);
        MIZU_LOG_ERROR("  Failed tests: {}", results.failed_tests);
    }
    else
    {
        MIZU_LOG_INFO("Render Tests Results for {}:", meta::enum_name(environment.graphics_api));
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

    const std::filesystem::path temp_directory = std::filesystem::temp_directory_path();
    const std::filesystem::path session_directory =
        temp_directory / std::format("Mizu_RenderTestsSession_{}", std::time(nullptr));

    for (RenderTestsInfo& test_info : render_tests_info)
    {
        test_info.execution_type = execution_type;
        test_info.session_path = session_directory;
        test_info.reference_images_path = std::filesystem::path{MIZU_RENDER_TESTS_REFERENCE_IMAGES_PATH};
    }

    std::vector<RenderTestsRunnerResults> results{};

    for (const RenderTestsInfo& tests_info : render_tests_info)
    {
        RenderTestsRunner runner{tests_info};
        runner.run_tests();

        if (execution_type == ExecutionType::CompareImages)
        {
            results.push_back(runner.get_results());
        }
    }

    int32_t return_code = 0;

    if (execution_type == ExecutionType::CompareImages)
    {
        for (size_t i = 0; i < results.size(); ++i)
        {
            const RenderTestsRunnerResults& result = results[i];
            const RenderTestEnvironment& environment = render_tests_info[i].environment;
            print_results(result, environment);

            if (result.failed_tests > 0)
            {
                return_code = 1;
            }
        }

        MIZU_LOG_INFO("Render test session results stored at: {}", session_directory.string());
    }

    return return_code;
}
