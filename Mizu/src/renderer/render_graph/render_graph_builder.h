#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

#include "renderer/buffers.h"
#include "renderer/cubemap.h"
#include "renderer/texture.h"

#include "renderer/abstraction/buffer_resource.h"
#include "renderer/abstraction/image_resource.h"

#include "renderer/render_graph/render_graph.h"
#include "renderer/render_graph/render_graph_dependencies.h"
#include "renderer/render_graph/render_graph_types.h"

#include "renderer/shader/shader_declaration.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu {

class RenderGraphBuilder {
  public:
    RenderGraphBuilder() = default;

    //
    // Resources
    //

    template <typename TextureT>
    RGTextureRef create_texture(decltype(TextureT::Description::dimensions) dimensions,
                                ImageFormat format,
                                SamplingOptions sampling) {
        static_assert(std::is_base_of_v<ITextureBase, TextureT>, "TextureT must inherit from ITextureBase");

        typename TextureT::Description desc{};
        desc.dimensions = dimensions;
        desc.format = format;

        const ImageDescription image_desc = TextureT::get_image_description(desc);

        RGImageDescription rg_desc{};
        rg_desc.desc = image_desc;
        rg_desc.sampling = sampling;

        auto id = RGTextureRef();
        m_transient_image_descriptions.insert({id, rg_desc});

        return id;
    }

    template <typename TextureT>
    RGTextureRef register_external_texture(const TextureT& texture) {
        static_assert(std::is_base_of_v<ITextureBase, TextureT>, "TextureT must inherit from ITextureBase");

        auto id = RGTextureRef();
        m_external_images.insert({id, texture.get_resource()});

        return id;
    }

    RGCubemapRef create_cubemap(glm::vec2 dimensions, ImageFormat format, SamplingOptions sampling);
    RGCubemapRef register_external_cubemap(const Cubemap& cubemap);

    template <typename T>
    RGBufferRef create_uniform_buffer() {
        RGBufferDescription desc{};
        desc.size = sizeof(T);

        auto id = RGBufferRef();
        m_transient_buffer_descriptions.insert({id, desc});

        return id;
    }

    RGBufferRef register_external_buffer(const UniformBuffer& ubo);

    RGFramebufferRef create_framebuffer(glm::uvec2 dimensions, const std::vector<RGTextureRef>& attachments);

    //
    // Passes
    //

    template <typename ShaderT>
    void add_pass(std::string name,
                  typename ShaderT::Parameters params,
                  RGGraphicsPipelineDescription pipeline_desc,
                  RGFramebufferRef framebuffer,
                  RGFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "ShaderT must inherit from ShaderDeclaration");

        const std::vector<ShaderDeclarationMemberInfo>& members = ShaderT::Parameters::get_members(params);

        const auto& shader = std::dynamic_pointer_cast<GraphicsShader>(ShaderT::get_shader());
        MIZU_ASSERT(shader != nullptr, "Shader is nullptr, did you forget to call IMPLEMENT_GRAPHICS_SHADER?");

#if MIZU_DEBUG
        validate_shader_declaration_members(*shader, members);
#endif

        RGRenderPassInfo value{};
        value.shader = shader;
        value.pipeline_desc = pipeline_desc;
        value.framebuffer = framebuffer;
        value.func = func;

        RGPassInfo info{};
        info.name = name;
        info.dependencies = create_dependencies(members);
        info.members = members;
        info.value = value;

        m_passes.push_back(info);
    }

    template <typename ShaderT>
    void add_pass(std::string name, typename ShaderT::Parameters params, RGFunction func) {
        static_assert(std::is_base_of_v<ShaderDeclaration<typename ShaderT::Parent>, ShaderT>,
                      "ShaderT must inherit from ShaderDeclaration");

        const std::vector<ShaderDeclarationMemberInfo>& members = ShaderT::Parameters::get_members(params);

        const auto& shader = std::dynamic_pointer_cast<ComputeShader>(ShaderT::get_shader());
        MIZU_ASSERT(shader != nullptr, "Shader is nullptr, did you forget to call IMPLEMENT_COMPUTE_SHADER?");

#if MIZU_DEBUG
        validate_shader_declaration_members(*shader, members);
#endif

        RGComputePassInfo value{};
        value.shader = shader;
        value.func = func;

        RGPassInfo info{};
        info.name = name;
        info.dependencies = create_dependencies(members);
        info.members = members;
        info.value = value;

        m_passes.push_back(info);
    }

    //
    // Compile
    //

    std::optional<RenderGraph> compile(std::shared_ptr<RenderCommandBuffer> command,
                                       RenderGraphDeviceMemoryAllocator& allocator);

  private:
    // Resources

    struct RGImageDescription {
        ImageDescription desc;
        SamplingOptions sampling;
    };

    std::unordered_map<RGImageRef, RGImageDescription> m_transient_image_descriptions;
    std::unordered_map<RGImageRef, std::shared_ptr<ImageResource>> m_external_images;

    struct RGBufferDescription {
        size_t size;
    };

    std::unordered_map<RGBufferRef, RGBufferDescription> m_transient_buffer_descriptions;
    std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>> m_external_buffers;

    struct RGFramebufferDescription {
        uint32_t width, height;
        std::vector<RGTextureRef> attachments;
    };
    std::unordered_map<RGFramebufferRef, RGFramebufferDescription> m_framebuffer_descriptions;

    // Passes

    struct RGRenderPassInfo {
        std::shared_ptr<GraphicsShader> shader;
        RGGraphicsPipelineDescription pipeline_desc;
        RGFramebufferRef framebuffer;

        RGFunction func;
    };

    struct RGComputePassInfo {
        std::shared_ptr<ComputeShader> shader;
        RGFunction func;
    };

    using RGPassInfoT = std::variant<RGRenderPassInfo, RGComputePassInfo>;
    struct RGPassInfo {
        std::string name;
        RenderGraphDependencies dependencies;
        std::vector<ShaderDeclarationMemberInfo> members;

        RGPassInfoT value;

        template <typename T>
        bool is_type() const {
            static_assert(std::is_convertible_v<T, RGPassInfoT>, "T is not compatible with RGPassInfoT variant");
            return std::holds_alternative<T>(value);
        }

        template <typename T>
        T get_value() const {
            MIZU_ASSERT(is_type<T>(), "Value is not of the requested type");
            return std::get<T>(value);
        }
    };
    std::vector<RGPassInfo> m_passes;

    // Helpers

    RenderGraphDependencies create_dependencies(const std::vector<ShaderDeclarationMemberInfo>& members);

    void validate_shader_declaration_members(const IShader& shader,
                                             const std::vector<ShaderDeclarationMemberInfo>& members);

    // Compile Helpers
    using RGImageMap = std::unordered_map<RGImageRef, std::shared_ptr<ImageResource>>;
    using RGBufferMap = std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>>;

    struct RGImageUsage {
        enum class Type {
            Sampled,
            Storage,
            Attachment,
        };

        Type type;
        size_t render_pass_idx = 0;
        RGImageRef value;
    };
    std::vector<RGImageUsage> get_image_usages(RGImageRef ref) const;

    struct RGBufferUsage {
        enum class Type {
            UniformBuffer,
            // TODO: StorageBuffer
        };

        Type type;
        size_t render_pass_idx = 0;
    };
    std::vector<RGBufferUsage> get_buffer_usages(RGBufferRef ref) const;

    using ResourceMemberInfoT = std::variant<std::shared_ptr<ImageResource>, std::shared_ptr<BufferResource>>;
    struct RGResourceMemberInfo {
        std::string name;
        uint32_t set;
        ResourceMemberInfoT value;
    };

    std::vector<RGResourceMemberInfo> create_members(const RGPassInfo& info,
                                                     const IShader& shader,
                                                     const RGImageMap& images,
                                                     const RGBufferMap& buffers) const;
    std::vector<std::shared_ptr<ResourceGroup>> create_resource_groups(
        const std::shared_ptr<IShader>& shader,
        const std::vector<RGResourceMemberInfo>& members,
        std::unordered_map<size_t, std::shared_ptr<ResourceGroup>>& checksum_to_resource_group) const;
};

} // namespace Mizu
