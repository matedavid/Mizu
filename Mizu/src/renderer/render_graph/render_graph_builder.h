#pragma once

#include <string_view>
#include <vector>

#include "core/uuid.h"
#include "renderer/abstraction/texture.h"
#include "renderer/render_graph/render_graph_dependencies.h"
#include "renderer/render_graph/render_graph_types.h"
#include "renderer/shader_declaration.h"
#include "utility/logging.h"

namespace Mizu {

class RenderGraphBuilder {
  public:
    RenderGraphBuilder() = default;

    RGTextureRef create_texture(uint32_t width, uint32_t height, ImageFormat format);
    RGTextureRef create_framebuffer(uint32_t width, uint32_t height, std::vector<RGTextureRef> attachments);

    RGTextureRef register_texture(std::shared_ptr<Texture2D> texture);

    template <typename ShaderT>
    void add_pass(std::string_view name,
                  const RGGraphicsPipelineDescription& pipeline_desc,
                  const ShaderT::Parameters& params,
                  RGFramebufferRef framebuffer,
                  RGFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "Type must be ShaderDeclaration");

        const size_t checksum = get_graphics_pipeline_checksum(pipeline_desc, typeid(ShaderT).name());

        auto it = m_pipeline_descriptions.find(checksum);
        if (it == m_pipeline_descriptions.end()) {
            m_pipeline_descriptions.insert({checksum, pipeline_desc});
        }

        // TODO: Check if shader declaration is valid

        RenderGraphDependencies dependencies;

        for (const ShaderDeclarationMemberInfo& member : ShaderT::Parameters::get_members(params)) {
            switch (member.mem_type) {
            case ShaderDeclarationMemberType::RGTexture2D: {
                dependencies.add_rg_texture2D(std::get<RGTextureRef>(member.value));
                break;
            }
            }
        }

        RGRenderPassCreateInfo info;
        info.name = name;
        info.pipeline_desc_id = checksum;
        info.shader = ShaderT::get_shader();
        info.dependencies = dependencies;
        info.framebuffer_id = framebuffer;
        info.func = std::move(func);

        m_render_pass_creation_list.push_back(info);
    }

  private:
    struct RGTextureCreateInfo {
        RGTextureRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        ImageFormat format = ImageFormat::BGRA8_SRGB;
    };
    std::vector<RGTextureCreateInfo> m_texture_creation_list;
    std::unordered_map<RGTextureRef, std::shared_ptr<Texture2D>> m_external_textures;

    struct RGFramebufferCreateInfo {
        RGFramebufferRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        std::vector<RGTextureRef> attachments{};
    };
    std::vector<RGFramebufferCreateInfo> m_framebuffer_creation_list;

    struct RGRenderPassCreateInfo {
        std::string name;
        size_t pipeline_desc_id;
        std::shared_ptr<GraphicsShader> shader;
        RenderGraphDependencies dependencies;
        RGFramebufferRef framebuffer_id;
        RGFunction func;
    };
    std::vector<RGRenderPassCreateInfo> m_render_pass_creation_list;

    std::unordered_map<size_t, RGGraphicsPipelineDescription> m_pipeline_descriptions;

    static size_t get_graphics_pipeline_checksum(const RGGraphicsPipelineDescription& desc,
                                                 const std::string& shader_name);

    friend class RenderGraph;
};

} // namespace Mizu