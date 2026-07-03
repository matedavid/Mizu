#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "runner/render_test.h"

enum class ExecutionType
{
    UpdateReferenceImages,
    CompareImages,
};

struct RenderTestsInfo
{
    RenderTestEnvironment environment{};
    std::span<RenderTest*> render_tests{};

    ExecutionType execution_type = ExecutionType::UpdateReferenceImages;
    std::filesystem::path session_path{};
    std::filesystem::path reference_images_path{};
};

struct RenderTestsRunnerResults
{
    uint32_t passed_tests = 0;
    uint32_t failed_tests = 0;
};

class RenderTestsRunner
{
  public:
    RenderTestsRunner(RenderTestsInfo info);
    ~RenderTestsRunner();

    void run_tests();

    const RenderTestsRunnerResults& get_results() const { return m_results; }

    static constexpr uint32_t TEST_WIDTH = 1280;
    static constexpr uint32_t TEST_HEIGHT = 720;
    static constexpr Mizu::ImageFormat TEST_IMAGE_FORMAT = Mizu::ImageFormat::R8G8B8A8_UNORM;

  private:
    void add_test_execution_pass(
        Mizu::RenderGraphBuilder& builder,
        RenderTest* render_test,
        const RenderTestExecutionEnvironment& environment) const;

    void add_texture_readback_pass(
        Mizu::RenderGraphBuilder& builder,
        Mizu::RenderGraphResource output_texture,
        Mizu::RenderGraphResource readback_buffer) const;

    Mizu::RenderGraphResource add_reference_texture_upload_pass(
        Mizu::RenderGraphBuilder& builder,
        Mizu::FrameLinearAllocator& frame_allocator,
        const RenderTest& render_test) const;

    void add_texture_compare_pass(
        Mizu::RenderGraphBuilder& builder,
        Mizu::RenderGraphResource pending_texture,
        Mizu::RenderGraphResource reference_texture,
        std::shared_ptr<Mizu::BufferResource> comparison_result_readback_buffer) const;

    void save_updated_reference_image(const RenderTest& render_test, const Mizu::BufferResource& image_readback_buffer)
        const;
    bool save_compare_images_result(
        const RenderTest& render_test,
        const Mizu::BufferResource& image_readback_buffer,
        const Mizu::BufferResource& result_readback_buffer) const;

    void save_image_to_disk(
        std::string_view filename,
        const uint8_t* data,
        uint32_t width,
        uint32_t height,
        uint32_t stride,
        uint32_t components) const;

    std::filesystem::path get_reference_image_path(const RenderTest& render_test) const;
    std::string get_full_test_name(const RenderTest& render_test) const;

    RenderTestsInfo m_info{};
    RenderTestsRunnerResults m_results{};
};
