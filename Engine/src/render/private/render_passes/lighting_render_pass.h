#pragma once

#include "render/render_graph/render_graph_types.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;
struct LightsData;

struct LightingData
{
    RenderGraphResource lighting_output;
};

struct LightCullingData
{
    struct GpuLightCullingInfo
    {
        glm::uvec2 num_tiles;
        uint32_t num_point_lights;
        uint32_t num_directional_lights;
    };

    GpuLightCullingInfo light_culling_info{};

    RenderGraphResource tile_visible_lights;
    FrameAllocation light_culling_info_allocation;
};

LightCullingData create_light_culling_data(
    RenderGraphBuilder& builder,
    uint32_t width,
    uint32_t height,
    const LightsData& lights_data,
    FrameLinearAllocator& frame_allocator);

void add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);
void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu