#pragma once

#include "render/render_graph/render_graph_types.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;

struct DepthData
{
    RenderGraphResource depth;
    bool depth_prepass_enabled;
};

void add_depth_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu