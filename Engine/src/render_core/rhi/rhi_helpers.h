#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class Material;
class Mesh;

namespace RHIHelpers
{

// TODO: Probably makes sense to move somewhere in renderer/
void draw_mesh(CommandBuffer& command, const Mesh& mesh);
void draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count);

// TODO: Probably makes sense to move somewhere in renderer/
void set_material(CommandBuffer& command, const Material& material);

glm::uvec3 compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size);

} // namespace RHIHelpers

} // namespace Mizu
