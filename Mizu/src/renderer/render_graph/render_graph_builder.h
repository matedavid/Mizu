#pragma once

#include <string_view>
#include <vector>

#include "core/uuid.h"
#include "renderer/abstraction/texture.h"
#include "renderer/render_graph/render_graph_dependencies.h"
#include "renderer/render_graph/render_graph_types.h"
#include "renderer/shader_declaration.h"
#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu {

#define CONVERT(variable, klass) std::dynamic_pointer_cast<klass>(variable)

class RenderGraphBuilder {
  public:
    RenderGraphBuilder() = default;

    RGTextureRef create_texture(uint32_t width, uint32_t height, ImageFormat format);
    RGFramebufferRef create_framebuffer(uint32_t width, uint32_t height, std::vector<RGTextureRef> attachments);

    RGTextureRef register_texture(std::shared_ptr<Texture2D> texture);
    RGUniformBufferRef register_uniform_buffer(std::shared_ptr<UniformBuffer> uniform_buffer);

    template <typename ShaderT>
    void add_pass(std::string_view name,
                  const RGGraphicsPipelineDescription& pipeline_desc,
                  const typename ShaderT::Parameters& params,
                  RGFramebufferRef framebuffer,
                  RGRenderFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "Type must be ShaderDeclaration");

        const size_t checksum = get_graphics_pipeline_checksum(pipeline_desc, typeid(ShaderT).name());

        auto it = m_pipeline_descriptions.find(checksum);
        if (it == m_pipeline_descriptions.end()) {
            m_pipeline_descriptions.insert({checksum, pipeline_desc});
        }

        const std::shared_ptr<GraphicsShader> shader = CONVERT(ShaderT::get_shader(), GraphicsShader);
        MIZU_ASSERT(shader != nullptr, "Shader is nullptr, did you forget to call IMPLEMENT_GRAPHICS_SHADER?");
        const auto members = ShaderT::Parameters::get_members(params);

#ifdef MIZU_DEBUG
        // Check that shader declaration members are valid
        validate_shader_declaration_members(shader, members);
#endif

        RGRenderPassCreateInfo value;
        value.pipeline_desc_id = checksum;
        value.shader = shader;
        value.framebuffer_id = framebuffer;
        value.func = std::move(func);

        RGPassCreateInfo info;
        info.name = name;
        info.dependencies = create_dependencies(members);
        info.members = members;
        info.value = value;

        m_pass_create_info_list.push_back(info);
    }

    template <typename ShaderT>
    void add_pass(std::string_view name, const typename ShaderT::Parameters& params, RGComputeFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "Type must be ShaderDeclaration");

        const std::shared_ptr<ComputeShader> shader = CONVERT(ShaderT::get_shader(), ComputeShader);
        MIZU_ASSERT(shader != nullptr, "Shader is nullptr, did you forget to call IMPLEMENT_COMPUTE_SHADER?");
        const auto members = ShaderT::Parameters::get_members(params);

#ifdef MIZU_DEBUG
        // Check that shader declaration members are valid
        validate_shader_declaration_members(shader, members);
#endif

        RGComputePassCreateInfo value;
        value.shader = shader;
        value.func = std::move(func);

        RGPassCreateInfo info;
        info.name = name;
        info.dependencies = create_dependencies(members);
        info.members = members;
        info.value = value;

        m_pass_create_info_list.push_back(info);
    }

  private:
    // Texture
    struct RGTextureCreateInfo {
        RGTextureRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        ImageFormat format = ImageFormat::BGRA8_SRGB;
    };
    std::vector<RGTextureCreateInfo> m_texture_creation_list;
    std::unordered_map<RGTextureRef, std::shared_ptr<Texture2D>> m_external_textures;

    // Uniform Buffer
    std::unordered_map<RGUniformBufferRef, std::shared_ptr<UniformBuffer>> m_external_uniform_buffers;

    // Framebuffer
    struct RGFramebufferCreateInfo {
        RGFramebufferRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        std::vector<RGTextureRef> attachments{};
    };
    std::vector<RGFramebufferCreateInfo> m_framebuffer_creation_list;

    // Pass Creation
    struct RGRenderPassCreateInfo {
        size_t pipeline_desc_id;
        std::shared_ptr<GraphicsShader> shader;
        RGFramebufferRef framebuffer_id;
        RGRenderFunction func;
    };

    struct RGComputePassCreateInfo {
        std::shared_ptr<ComputeShader> shader;
        RGComputeFunction func;
    };

    using RGPassCreateInfoT = std::variant<RGRenderPassCreateInfo, RGComputePassCreateInfo>;

    struct RGPassCreateInfo {
        std::string name;
        RenderGraphDependencies dependencies;
        std::vector<ShaderDeclarationMemberInfo> members;

        RGPassCreateInfoT value;

        bool is_render_pass() const { return std::holds_alternative<RGRenderPassCreateInfo>(value); }
        bool is_compute_pass() const { return std::holds_alternative<RGComputePassCreateInfo>(value); }
    };

    std::vector<RGPassCreateInfo> m_pass_create_info_list;

    // Pipeline
    std::unordered_map<size_t, RGGraphicsPipelineDescription> m_pipeline_descriptions;

    static size_t get_graphics_pipeline_checksum(const RGGraphicsPipelineDescription& desc,
                                                 const std::string& shader_name);

    // Validation
    void validate_shader_declaration_members(const std::shared_ptr<GraphicsShader>& shader,
                                             const std::vector<ShaderDeclarationMemberInfo>& members);

    [[nodiscard]] static RenderGraphDependencies create_dependencies(
        const std::vector<ShaderDeclarationMemberInfo>& members);

    friend class RenderGraph;
};

} // namespace Mizu
