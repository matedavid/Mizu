#include "render_graph_builder.h"

#include "utility/assert.h"

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

RGUniformBufferRef RenderGraphBuilder::register_uniform_buffer(std::shared_ptr<UniformBuffer> uniform_buffer) {
    const auto id = RGUniformBufferRef();
    m_external_uniform_buffers.insert({id, std::move(uniform_buffer)});

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

void RenderGraphBuilder::validate_shader_declaration_members(const std::shared_ptr<GraphicsShader>& shader,
                                                             const std::vector<ShaderDeclarationMemberInfo>& members) {
    const auto properties = shader->get_properties();

    const auto has_member = [&](std::string name, ShaderDeclarationMemberType type) -> bool {
        return std::find_if(members.begin(),
                            members.end(),
                            [&](const auto& member) { return member.mem_name == name && member.mem_type == type; })
               != members.end();
    };

    bool one_property_not_found = false;

    for (const auto& property : properties) {
        std::string property_name;
        bool found = false;

        if (std::holds_alternative<ShaderTextureProperty>(property)) {
            auto p = std::get<ShaderTextureProperty>(property);
            property_name = p.name;

            found = has_member(property_name, ShaderDeclarationMemberType::RGTexture2D);
        } else if (std::holds_alternative<ShaderUniformBufferProperty>(property)) {
            auto p = std::get<ShaderUniformBufferProperty>(property);
            property_name = p.name;

            found = has_member(property_name, ShaderDeclarationMemberType::RGUniformBuffer);
        }

        if (!found) {
            one_property_not_found = true;
            MIZU_LOG_ERROR("Shader property with name '{}' not present in shader declaration", property_name);
        }
    }

    MIZU_ASSERT(!one_property_not_found, "Shader declaration does not match shader");
}

} // namespace Mizu