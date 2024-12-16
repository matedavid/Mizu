#include "rhi_helpers.h"

#include "render_core/resources/mesh.h"
#include "render_core/rhi/command_buffer.h"

namespace Mizu
{

void RHIHelpers::draw_mesh(RenderCommandBuffer& command, const Mesh& mesh)
{
    command.draw_indexed(mesh.vertex_buffer(), mesh.index_buffer());
}

} // namespace Mizu
