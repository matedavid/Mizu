#pragma once

#include <cstdint>

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class Material;
class Mesh;

void draw_mesh(CommandBuffer& command, const Mesh& mesh);
void draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count);

void set_material(CommandBuffer& command, const Material& material);

} // namespace Mizu