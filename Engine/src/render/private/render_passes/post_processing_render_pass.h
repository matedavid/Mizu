#pragma once

#include "render/render_graph/render_graph_types.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;

void add_tonemapping_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu
