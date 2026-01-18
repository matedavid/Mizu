#include "render/core/render_utils.h"

#include "render_core/rhi/command_buffer.h"

#include "render/material/material.h"
#include "render/model/mesh.h"

namespace Mizu
{

void draw_mesh(CommandBuffer& command, const Mesh& mesh)
{
    draw_mesh_instanced(command, mesh, 1);
}

void draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count)
{
    const std::string& mesh_name = mesh.get_name();

    if (!mesh_name.empty())
        command.begin_gpu_marker(mesh_name);

    command.draw_indexed_instanced(*mesh.vertex_buffer(), *mesh.index_buffer(), instance_count);

    if (!mesh_name.empty())
        command.end_gpu_marker();
}

void set_material(CommandBuffer& command, const Material& material)
{
    MIZU_ASSERT(command.get_active_framebuffer() != nullptr, "CommandBuffer has no bound RenderPass");
    MIZU_ASSERT(material.is_baked(), "Material has not been baked");

    for (const MaterialResourceGroup& material_rg : material.get_resource_groups())
    {
        command.bind_resource_group(material_rg.resource_group, material_rg.set);
    }
}

} // namespace Mizu