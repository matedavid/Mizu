#pragma once

namespace Mizu
{

// Forward declarations
class RenderCommandBuffer;
class Mesh;

namespace RHIHelpers
{

void draw_mesh(RenderCommandBuffer& command, const Mesh& mesh);

}

} // namespace Mizu
