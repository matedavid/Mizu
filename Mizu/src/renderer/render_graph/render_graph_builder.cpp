#include "render_graph_builder.h"

namespace Mizu {

RGTextureRef RenderGraphBuilder::create_texture(uint32_t width, uint32_t height, ImageFormat format) {
    RGTextureCreateInfo info;
    info.id = RGTextureRef();
    info.width = width;
    info.height = height;
    info.format = format;

    m_texture_creation_list.push_back(info);

    return info.id;
}

RGFramebufferRef RenderGraphBuilder::create_framebuffer(uint32_t width,
                                                    uint32_t height,
                                                    std::vector<RGTextureRef> attachments) {
    RGFramebufferCreateInfo info;
    info.id = RGFramebufferRef();
    info.width = width;
    info.height = height;
    info.attachments = std::move(attachments);

    m_framebuffer_creation_list.push_back(info);

    return info.id;
}

RGTextureRef RenderGraphBuilder::register_texture(std::shared_ptr<Texture2D> texture) {
    const auto id = RGTextureRef();
    m_external_textures.insert({id, std::move(texture)});

    return id;
}

template <class T>
size_t calc_checksum(const T& data) {
    return std::hash<T>()(data);
}

#define CALC_CHECKSUM(d) checksum ^= calc_checksum(d) << (pos++)

size_t RenderGraphBuilder::get_graphics_pipeline_checksum(const RGGraphicsPipelineDescription& desc,
                                                          const std::string& shader_name) {
    size_t checksum = 0;
    size_t pos = 0;

    // Shader
    { CALC_CHECKSUM(shader_name); }

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