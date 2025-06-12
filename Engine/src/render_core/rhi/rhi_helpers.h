#pragma once

#include <memory>

#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class Material;
class Mesh;
class SamplerState;
struct SamplingOptions;

namespace RHIHelpers
{

std::shared_ptr<SamplerState> get_sampler_state(const SamplingOptions& options);

void draw_mesh(CommandBuffer& command, const Mesh& mesh);

void set_pipeline_state(CommandBuffer& command, const GraphicsPipeline::Description& pipeline_desc);
void set_pipeline_state(CommandBuffer& command, const ComputePipeline::Description& pipeline_desc);

void set_material(CommandBuffer& command,
                  const Material& material,
                  const GraphicsPipeline::Description& pipeline_desc = {});

glm::uvec3 compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size);

} // namespace RHIHelpers

} // namespace Mizu
