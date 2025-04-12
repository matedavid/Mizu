#include "rhi_helpers.h"

#include "render_core/resources/material.h"
#include "render_core/resources/mesh.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_group.h"

namespace Mizu
{

std::shared_ptr<SamplerState> RHIHelpers::get_sampler_state(const SamplingOptions& options)
{
    return Renderer::get_sampler_state_cache()->get_sampler_state(options);
}

void RHIHelpers::draw_mesh(RenderCommandBuffer& command, const Mesh& mesh)
{
    command.draw_indexed(*mesh.vertex_buffer(), *mesh.index_buffer());
}

void RHIHelpers::set_pipeline_state(RenderCommandBuffer& command, const GraphicsPipeline::Description& pipeline_desc)
{
    MIZU_ASSERT(command.get_current_render_pass() != nullptr, "RenderCommandBuffer has no bound RenderPass");

    GraphicsPipeline::Description local_desc = pipeline_desc;
    local_desc.target_framebuffer = command.get_current_render_pass()->get_framebuffer();

    const std::shared_ptr<GraphicsPipeline>& pipeline = Renderer::get_pipeline_cache()->get_pipeline(local_desc);
    MIZU_ASSERT(pipeline != nullptr, "Pipeline is nullptr");
    command.bind_pipeline(pipeline);
}

void RHIHelpers::set_material(RenderCommandBuffer& command,
                              const Material& material,
                              const GraphicsPipeline::Description& pipeline_desc)
{
    MIZU_ASSERT(command.get_current_render_pass() != nullptr, "RenderCommandBuffer has no bound RenderPass");
    MIZU_ASSERT(material.is_baked(), "Material has not been baked");

    GraphicsPipeline::Description local_desc = pipeline_desc;
    local_desc.shader = material.get_shader();

    set_pipeline_state(command, local_desc);

    for (const std::shared_ptr<ResourceGroup>& resource_group : material.get_resource_groups())
    {
        command.bind_resource_group(resource_group, resource_group->currently_baked_set());
    }
}

glm::uvec3 RHIHelpers::compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size)
{
    return {(thread_count.x + group_size.x - 1) / group_size.x,
            (thread_count.y + group_size.y - 1) / group_size.y,
            (thread_count.z + group_size.z - 1) / group_size.z};
}

} // namespace Mizu
