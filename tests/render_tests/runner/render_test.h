#pragma once

#include <cstdint>
#include <string_view>

#include "render/frame_linear_allocator.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph/render_graph_types.h"
#include "render_core/rhi/device.h"

struct RenderTestEnvironment
{
    Mizu::GraphicsApi graphics_api{};
};

struct RenderTestExecutionEnvironment
{
    Mizu::GraphicsApi graphics_api{};
    Mizu::FrameLinearAllocator* frame_allocator = nullptr;
    uint32_t output_width = 0, output_height = 0;
    Mizu::RenderGraphResource output_texture;
};

class RenderTest
{
  public:
    virtual ~RenderTest() {}

    virtual std::string_view get_test_group_name() const = 0;
    virtual std::string_view get_test_name() const = 0;

    virtual bool should_run_test(const RenderTestEnvironment& environment) const = 0;

    virtual void prepare_test([[maybe_unused]] const RenderTestExecutionEnvironment& environment) {}
    virtual void cleanup_test() {}

    virtual void run_test(Mizu::RenderGraphBuilder& builder, const RenderTestExecutionEnvironment& environment) = 0;
};