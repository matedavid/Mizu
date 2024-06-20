#include "render_graph.h"

#include "renderer/abstraction/buffers.h"
#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/texture.h"

namespace Mizu {

RenderGraph::RenderGraph() {}

RenderGraph::~RenderGraph() {}

void RenderGraph::register_texture(std::shared_ptr<Texture2D> texture) {}

void RenderGraph::register_uniform_buffer(std::shared_ptr<UniformBuffer> ub) {}

void RenderGraph::add_pass(std::string_view name,
                           const GraphicsPipeline::Description& pipeline_description,
                           std::function<void(std::shared_ptr<ICommandBuffer>)> func) {}

void RenderGraph::execute() {}

} // namespace Mizu
