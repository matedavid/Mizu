#pragma once

#include "render_core/rhi/graphics_pipeline.h"

namespace Mizu
{

// Forward declarations
class RenderCommandBuffer;
class Material;
class Mesh;
class SamplerState;
struct SamplingOptions;

namespace RHIHelpers
{

std::shared_ptr<SamplerState> get_sampler_state(const SamplingOptions& options);

void draw_mesh(RenderCommandBuffer& command, const Mesh& mesh);

void set_pipeline_state(RenderCommandBuffer& command, const GraphicsPipeline::Description& pipeline_desc);

void set_material(RenderCommandBuffer& command,
                  const Material& material,
                  const GraphicsPipeline::Description& pipeline_desc = {});

} // namespace RHIHelpers

} // namespace Mizu
