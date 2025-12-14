#include "rhi_helpers.h"

#include "renderer/material/material.h"
#include "renderer/model/mesh.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

void RHIHelpers::draw_mesh(CommandBuffer& command, const Mesh& mesh)
{
    draw_mesh_instanced(command, mesh, 1);
}

void RHIHelpers::draw_mesh_instanced(CommandBuffer& command, const Mesh& mesh, uint32_t instance_count)
{
    const std::string& mesh_name = mesh.get_name();

    if (!mesh_name.empty())
        command.begin_gpu_marker(mesh_name);

    command.draw_indexed_instanced(*mesh.vertex_buffer(), *mesh.index_buffer(), instance_count);

    if (!mesh_name.empty())
        command.end_gpu_marker();
}

void RHIHelpers::set_material(CommandBuffer& command, const Material& material)
{
    MIZU_ASSERT(command.get_active_framebuffer() != nullptr, "CommandBuffer has no bound RenderPass");
    MIZU_ASSERT(material.is_baked(), "Material has not been baked");

    for (const MaterialResourceGroup& material_rg : material.get_resource_groups())
    {
        command.bind_resource_group(material_rg.resource_group, material_rg.set);
    }
}

glm::uvec3 RHIHelpers::compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size)
{
    return {
        (thread_count.x + group_size.x - 1) / group_size.x,
        (thread_count.y + group_size.y - 1) / group_size.y,
        (thread_count.z + group_size.z - 1) / group_size.z};
}

} // namespace Mizu
