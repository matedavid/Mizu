#include "render_graph.h"

#include "renderer/abstraction/buffers.h"
#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/framebuffer.h"
#include "renderer/abstraction/render_pass.h"
#include "renderer/abstraction/texture.h"

namespace Mizu {

RenderGraph::RenderGraph() {
    m_command_buffer = RenderCommandBuffer::create();
}

RenderGraph::~RenderGraph() {}

void RenderGraph::register_texture(std::shared_ptr<Texture2D> texture) {}

void RenderGraph::register_uniform_buffer(std::shared_ptr<UniformBuffer> ub) {}

void RenderGraph::add_pass(std::string_view name,
                           const GraphicsPipeline::Description& pipeline_description,
                           std::shared_ptr<Framebuffer> framebuffer,
                           RGFunction func) {
    const size_t checksum = get_graphics_pipeline_checksum(pipeline_description);

    auto it = m_graphic_pipelines.find(checksum);
    if (it == m_graphic_pipelines.end()) {
        const auto pipeline = GraphicsPipeline::create(pipeline_description);
        it = m_graphic_pipelines.insert({checksum, pipeline}).first;
    }

    RenderPass::Description render_pass_desc{};
    render_pass_desc.debug_name = name;
    render_pass_desc.target_framebuffer = framebuffer;

    m_render_passes.push_back(RGRenderPassDescription{
        .render_pass = RenderPass::create(render_pass_desc),
        .pipeline = it->second,
        .function = func,
    });
}

void RenderGraph::execute(const CommandBufferSubmitInfo& submit_info) {
    m_command_buffer->begin();

    for (const auto& pass : m_render_passes) {
        m_command_buffer->begin_render_pass(pass.render_pass);
        m_command_buffer->bind_pipeline(pass.pipeline);

        pass.function(m_command_buffer);

        m_command_buffer->end_render_pass(pass.render_pass);
    }

    m_command_buffer->end();

    m_command_buffer->submit(submit_info);
}

template <class T>
size_t calc_checksum(const T& data) {
    return std::hash<T>()(data);
}

#define CALC_CHECKSUM(d) checksum ^= calc_checksum(d) << (pos++)

size_t RenderGraph::get_graphics_pipeline_checksum(const GraphicsPipeline::Description& desc) {
    size_t checksum = 0;
    size_t pos = 0;

    // RasterizationState
    {
        const RasterizationState& state = desc.rasterization;

        CALC_CHECKSUM(state.rasterizer_discard);
        CALC_CHECKSUM(state.polygon_mode);
        CALC_CHECKSUM(state.cull_mode);
        CALC_CHECKSUM(state.front_face);
        CALC_CHECKSUM(state.front_face);

        CALC_CHECKSUM(state.depth_bias.enabled);
        CALC_CHECKSUM(state.depth_bias.constant_factor);
        CALC_CHECKSUM(state.depth_bias.clamp);
        CALC_CHECKSUM(state.depth_bias.slope_factor);
    }

    // DepthStencilState
    {
        const DepthStencilState& state = desc.depth_stencil;

        CALC_CHECKSUM(state.depth_write);
        CALC_CHECKSUM(state.depth_write);
        CALC_CHECKSUM(state.depth_compare_op);
        CALC_CHECKSUM(state.depth_bounds_test);
        CALC_CHECKSUM(state.min_depth_bounds);
        CALC_CHECKSUM(state.max_depth_bounds);
        CALC_CHECKSUM(state.stencil_test);
    }

    // ColorBlendState
    {
        const ColorBlendState& state = desc.color_blend;

        CALC_CHECKSUM(state.logic_op_enable);
    }

    return checksum;
}

} // namespace Mizu
