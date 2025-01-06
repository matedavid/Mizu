#include "rhi_helpers.h"

#include "render_core/resources/mesh.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

void RHIHelpers::draw_mesh(RenderCommandBuffer& command, const Mesh& mesh)
{
    command.draw_indexed(mesh.vertex_buffer(), mesh.index_buffer());
}

void RHIHelpers::set_pipeline_state(RenderCommandBuffer& command, const GraphicsPipeline::Description& pipeline_desc)
{
    const std::shared_ptr<GraphicsPipeline>& pipeline = Renderer::get_pipeline_cache()->get_pipeline(pipeline_desc);
    MIZU_ASSERT(pipeline != nullptr, "Pipeline is nullptr");
    command.bind_pipeline(pipeline);
}

} // namespace Mizu
