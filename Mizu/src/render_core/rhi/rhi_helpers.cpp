#include "rhi_helpers.h"

#include "render_core/resources/mesh.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

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

} // namespace Mizu
