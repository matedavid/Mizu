#pragma once

#include <cstdint>

#include "render/render_graph/render_graph_types.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;

struct CascadedShadowData
{
    RenderGraphResource cascaded_shadow_atlas;
    FrameAllocation cascade_splits_allocation;
    FrameAllocation cascade_light_space_matrices_allocation;
    uint32_t num_shadow_casting_directional_lights = 0;
};

CascadedShadowData create_cascaded_shadow_data(
    RenderGraphBuilder& builder,
    FrameLinearAllocator& frame_allocator);

void add_cascaded_shadow_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu