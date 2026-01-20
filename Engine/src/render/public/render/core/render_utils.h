#pragma once

#include <cstdint>

#include "mizu_render_module.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;
class Material;
class Mesh;

MIZU_RENDER_API void draw_mesh(CommandBuffer& command, const Mesh& mesh);
MIZU_RENDER_API void draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count);

MIZU_RENDER_API void set_material(CommandBuffer& command, const Material& material);

} // namespace Mizu