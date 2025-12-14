#pragma once

#include <memory>

#include "render_core/rhi/pipeline.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class Material;
class Mesh;
class SamplerState;
struct SamplerStateDescription;

namespace RHIHelpers
{

std::shared_ptr<SamplerState> get_sampler_state(const SamplerStateDescription& options);

// TODO: Probably makes sense to move somewhere in renderer/
void draw_mesh(CommandBuffer& command, const Mesh& mesh);
void draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count);

// TODO: Probably makes sense to move somewhere in renderer/
void set_material(CommandBuffer& command, const Material& material);

glm::uvec3 compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size);

} // namespace RHIHelpers

} // namespace Mizu
