#include "render/utils/render_utils.h"

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
    MIZU_ASSERT(command.is_render_pass_active(), "CommandBuffer has no bound RenderPass");
    MIZU_ASSERT(material.is_baked(), "Material has not been baked");

    for (const MaterialDescriptorSet& material_ds : material.get_resource_groups())
    {
        command.bind_descriptor_set(material_ds.descriptor_set, material_ds.set);
    }
}

} // namespace Mizu
