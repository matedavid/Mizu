#pragma once

#include <string_view>
#include <vector>

#include "core/uuid.h"

#include "renderer/abstraction/cubemap.h"
#include "renderer/abstraction/texture.h"
#include "renderer/render_graph/render_graph_dependencies.h"
#include "renderer/render_graph/render_graph_types.h"
#include "renderer/shader/material_shader.h"
#include "renderer/shader/shader_declaration.h"

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
    RGCubemapRef register_cubemap(std::shared_ptr<Cubemap> cubemap);
    RGUniformBufferRef register_uniform_buffer(std::shared_ptr<UniformBuffer> uniform_buffer);

    template <typename ShaderT>
    void add_pass(std::string_view name,
                  const RGGraphicsPipelineDescription& pipeline_desc,
                  const typename ShaderT::Parameters& params,
                  RGFramebufferRef framebuffer,
                  RGFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "ShaderT must inherit from ShaderDeclaration");

        const size_t checksum = register_graphics_pipeline(pipeline_desc, typeid(ShaderT).name());

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

    template <typename MatShaderT>
    void add_pass(std::string_view name,
                  const RGGraphicsPipelineDescription& pipeline_desc,
                  const typename MatShaderT::Parameters& params,
                  RGFramebufferRef framebuffer,
                  RGMaterialFunction func) {
        static_assert(std::is_base_of_v<MaterialShader<typename MatShaderT::Parent>, MatShaderT>,
                      "MatShaderT must inherit from MaterialShader");

        const size_t checksum = register_graphics_pipeline(pipeline_desc, typeid(MatShaderT).name());

        const std::shared_ptr<GraphicsShader> shader = CONVERT(MatShaderT::get_shader(), GraphicsShader);
        MIZU_ASSERT(shader != nullptr, "Shader is nullptr, did you forget to call IMPLEMENT_GRAPHICS_SHADER?");
        const auto members = MatShaderT::Parameters::get_members(params);

#ifdef MIZU_DEBUG
        // Check that shader declaration members are valid
        auto validate_members = MatShaderT::Parameters::get_members(params);

        for (const MaterialParameterInfo& mat_param : MatShaderT::MaterialParameters::get_members({})) {
            validate_members.push_back(ShaderDeclarationMemberInfo{
                .mem_name = mat_param.param_name,
                .mem_type = mat_param.param_type,
            });
        }

        validate_shader_declaration_members(shader, validate_members);
#endif

        RGMaterialPassCreateInfo value;
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
    void add_pass(std::string_view name, const typename ShaderT::Parameters& params, RGFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "ShaderT must inherit from ShaderDeclaration");

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

    // Cubemap
    struct RGCubemapCreateInfo {
        RGTextureRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        ImageFormat format = ImageFormat::BGRA8_SRGB;
    };
    std::vector<RGCubemapCreateInfo> m_cubemap_creation_list;
    std::unordered_map<RGCubemapRef, std::shared_ptr<Cubemap>> m_external_cubemaps;

    // Uniform Buffer
    std::unordered_map<RGUniformBufferRef, std::shared_ptr<UniformBuffer>> m_external_uniform_buffers;

    // Framebuffer
    struct RGFramebufferCreateInfo {
        RGFramebufferRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        std::vector<RGTextureRef> attachments{};
    };
    std::unordered_map<RGFramebufferRef, RGFramebufferCreateInfo> m_framebuffer_info;

    // Pass Creation
    struct RGRenderPassCreateInfo {
        size_t pipeline_desc_id;
        std::shared_ptr<GraphicsShader> shader;
        RGFramebufferRef framebuffer_id;
        RGFunction func;
    };

    struct RGMaterialPassCreateInfo {
        size_t pipeline_desc_id;
        std::shared_ptr<GraphicsShader> shader;
        RGFramebufferRef framebuffer_id;
        RGMaterialFunction func;
    };

    struct RGComputePassCreateInfo {
        std::shared_ptr<ComputeShader> shader;
        RGFunction func;
    };

    using RGPassCreateInfoT = std::variant<RGRenderPassCreateInfo, RGMaterialPassCreateInfo, RGComputePassCreateInfo>;

    struct RGPassCreateInfo {
        std::string name;
        RenderGraphDependencies dependencies;
        std::vector<ShaderDeclarationMemberInfo> members;

        RGPassCreateInfoT value;

        bool is_render_pass() const { return std::holds_alternative<RGRenderPassCreateInfo>(value); }
        bool is_material_pass() const { return std::holds_alternative<RGMaterialPassCreateInfo>(value); }
        bool is_compute_pass() const { return std::holds_alternative<RGComputePassCreateInfo>(value); }
    };

    std::vector<RGPassCreateInfo> m_pass_create_info_list;

    // Pipeline
    std::unordered_map<size_t, RGGraphicsPipelineDescription> m_pipeline_descriptions;

    size_t register_graphics_pipeline(const RGGraphicsPipelineDescription& desc, const std::string& shader_name);
    static size_t get_graphics_pipeline_checksum(const RGGraphicsPipelineDescription& desc,
                                                 const std::string& shader_name);

    // Validation
    void validate_shader_declaration_members(const std::shared_ptr<IShader>& shader,
                                             const std::vector<ShaderDeclarationMemberInfo>& members);

    [[nodiscard]] static RenderGraphDependencies create_dependencies(
        const std::vector<ShaderDeclarationMemberInfo>& members);

    friend class RenderGraph;
};

} // namespace Mizu
