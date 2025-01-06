#pragma once

#include "render_core/rhi/graphics_pipeline.h"

namespace Mizu
{

// Forward declarations
class RenderCommandBuffer;
class Mesh;

namespace RHIHelpers
{

void draw_mesh(RenderCommandBuffer& command, const Mesh& mesh);

void set_pipeline_state(RenderCommandBuffer& command, const GraphicsPipeline::Description& pipeline_desc);

} // namespace RHIHelpers

} // namespace Mizu
