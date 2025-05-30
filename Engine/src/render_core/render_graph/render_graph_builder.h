#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

#include "render_core/resources/buffers.h"
#include "render_core/resources/cubemap.h"
#include "render_core/resources/texture.h"

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

#include "render_core/render_graph/render_graph.h"
#include "render_core/render_graph/render_graph_dependencies.h"
#include "render_core/render_graph/render_graph_types.h"
#include "render_core/render_graph/render_graph_utils.h"

#include "render_core/shader/shader_declaration.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

struct RGExternalTextureParams
{
    ImageResourceState input_state = ImageResourceState::ShaderReadOnly;
    ImageResourceState output_state = ImageResourceState::ShaderReadOnly;
};

template <typename T>
concept IsValidParametersType = requires(T t) {
    { t.get_members({}) } -> std::same_as<std::vector<ShaderParameterMemberInfo>>;
};

class RenderGraphBuilder
{
  public:
    RenderGraphBuilder() = default;

    //
    // Resources
    //

    template <typename TextureT>
    RGTextureRef create_texture(decltype(TextureT::Description::dimensions) dimensions,
                                ImageFormat format,
                                std::string name = "")
    {
        static_assert(std::is_base_of_v<ITextureBase, TextureT>, "TextureT must inherit from ITextureBase");

        typename TextureT::Description texture_desc{};
        texture_desc.dimensions = dimensions;
        texture_desc.format = format;
        texture_desc.name = name;

        return create_texture<TextureT>(texture_desc);
    }

    template <typename TextureT, typename T>
    RGTextureRef create_texture(decltype(TextureT::Description::dimensions) dimensions,
                                ImageFormat format,
                                const std::vector<T>& data,
                                std::string name = "")

    {
        static_assert(std::is_base_of_v<ITextureBase, TextureT>, "TextureT must inherit from ITextureBase");

        typename TextureT::Description texture_desc{};
        texture_desc.dimensions = dimensions;
        texture_desc.format = format;
        texture_desc.name = name;

        const ImageDescription image_desc = TextureT::get_image_description(texture_desc);

        RGImageDescription desc{};
        desc.image_desc = image_desc;
        desc.data = std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data.data()),
                                         reinterpret_cast<const uint8_t*>(data.data()) + data.size() * sizeof(T));

        auto id = RGTextureRef();
        m_transient_image_descriptions.insert({id, desc});

        return id;
    }

    template <typename TextureT>
    RGTextureRef create_texture(const typename TextureT::Description& texture_desc)
    {
        static_assert(std::is_base_of_v<ITextureBase, TextureT>, "TextureT must inherit from ITextureBase");

        const ImageDescription image_desc = TextureT::get_image_description(texture_desc);

        RGImageDescription desc{};
        desc.image_desc = image_desc;

        auto id = RGTextureRef();
        m_transient_image_descriptions.insert({id, desc});

        return id;
    }

    template <typename TextureT>
    RGTextureRef register_external_texture(const TextureT& texture, RGExternalTextureParams params = {})
    {
        static_assert(std::is_base_of_v<ITextureBase, TextureT>, "TextureT must inherit from ITextureBase");

        RGExternalImageDescription desc{};
        desc.resource = texture.get_resource();
        desc.input_state = params.input_state;
        desc.output_state = params.output_state;

        auto id = RGTextureRef();
        m_external_image_descriptions.insert({id, desc});

        return id;
    }

    RGCubemapRef create_cubemap(glm::vec2 dimensions, ImageFormat format, std::string name = "");
    RGCubemapRef create_cubemap(const Cubemap::Description& cubemap_desc);
    RGCubemapRef register_external_cubemap(const Cubemap& cubemap, RGExternalTextureParams params = {});

    RGImageViewRef create_image_view(RGImageRef image, ImageResourceViewRange range = {});

    template <typename T>
    RGUniformBufferRef create_uniform_buffer(const T& data, std::string name = "")
    {
        BufferDescription buffer_desc = UniformBuffer::get_buffer_description(sizeof(T), name);

        if (buffer_desc.size == 0)
        {
            // Cannot create empty buffer, if size is zero set to 1
            // TODO: Rethink this approach
            buffer_desc.size = 1;
        }

        RGBufferDescription desc{};
        desc.buffer_desc = buffer_desc;
        desc.data = std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(&data),
                                         reinterpret_cast<const uint8_t*>(&data) + sizeof(T));

        auto id = RGUniformBufferRef();
        m_transient_buffer_descriptions.insert({id, desc});

        return id;
    }

    RGUniformBufferRef register_external_buffer(const UniformBuffer& ubo);

    RGStorageBufferRef create_storage_buffer(uint64_t size, std::string name = "");

    template <typename T>
    RGStorageBufferRef create_storage_buffer(const std::vector<T>& data, std::string name = "")
    {
        BufferDescription buffer_desc = StorageBuffer::get_buffer_description(sizeof(T) * data.size(), name);

        if (buffer_desc.size == 0)
        {
            // Cannot create empty buffer, if size is zero set to 1
            // TODO: Rethink this approach
            buffer_desc.size = 1;
        }

        RGBufferDescription desc{};
        desc.buffer_desc = buffer_desc;
        if (!data.empty())
        {
            desc.data = std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data.data()),
                                             reinterpret_cast<const uint8_t*>(data.data()) + buffer_desc.size);
        }

        auto id = RGStorageBufferRef();
        m_transient_buffer_descriptions.insert({id, desc});

        return id;
    }

    RGStorageBufferRef register_external_buffer(const StorageBuffer& ssbo);

    RGFramebufferRef create_framebuffer(glm::uvec2 dimensions, const std::vector<RGImageViewRef>& attachments);

    void start_debug_label(std::string_view name);
    void end_debug_label();

    //
    // Passes
    //

    /*
     * Types:
     * 1. Params + framebuffer
     * 2. Params
     * 3. Graphics pass (shader + params + framebuffer + pipeline)
     * 4. Compute pass (shader + params)
     */

    template <typename ParamsT>
    void add_pass(std::string name, const ParamsT& params, RGFramebufferRef framebuffer, RGFunction func)
    {
        static_assert(IsValidParametersType<ParamsT>, "ParamsT must be a valid parameters type");

        const std::vector<ShaderParameterMemberInfo>& members = ParamsT::get_members(params);

        RGRenderPassNoPipelineInfo value{};
        value.framebuffer = framebuffer;

        RGPassInfo info{};
        info.name = name;
        info.inputs = create_inputs(members);
        info.members = members;
        info.func = func;
        info.value = value;

        m_passes.push_back(info);
    }

    template <typename ParamsT>
    void add_pass(std::string name, const ParamsT& params, RGFunction func)
    {
        static_assert(IsValidParametersType<ParamsT>, "ParamsT must be a valid parameters type");

        const std::vector<ShaderParameterMemberInfo>& members = ParamsT::get_members(params);

        RGPassInfo info{};
        info.name = name;
        info.inputs = create_inputs(members);
        info.members = members;
        info.func = func;
        info.value = RGNullPassInfo{};

        m_passes.push_back(info);
    }

    template <typename ShaderT>
    void add_pass(std::string name,
                  [[maybe_unused]] const ShaderT& shader_t,
                  const typename ShaderT::Parameters& params,
                  RGGraphicsPipelineDescription pipeline_desc,
                  RGFramebufferRef framebuffer,
                  RGFunction func)
    {
        static_assert(std::is_base_of_v<ShaderDeclaration, ShaderT>, "ShaderT must inherit from ShaderDeclaration");

        const std::vector<ShaderParameterMemberInfo>& members = ShaderT::Parameters::get_members(params);

        const auto& shader = std::dynamic_pointer_cast<GraphicsShader>(ShaderT::get_shader());

        RGRenderPassInfo value{};
        value.shader = shader;
        value.pipeline_desc = pipeline_desc;
        value.framebuffer = framebuffer;

        RGPassInfo info{};
        info.name = name;
        info.inputs = create_inputs(members);
        info.members = members;
        info.func = func;
        info.value = value;

        m_passes.push_back(info);
    }

    template <typename ShaderT>
    void add_pass(std::string name,
                  [[maybe_unused]] const ShaderT& shader_t,
                  const typename ShaderT::Parameters& params,
                  RGFunction func)
    {
        static_assert(std::is_base_of_v<ShaderDeclaration, ShaderT>, "ShaderT must inherit from ShaderDeclaration");

        const std::vector<ShaderParameterMemberInfo>& members = ShaderT::Parameters::get_members(params);

        const auto& shader = std::dynamic_pointer_cast<ComputeShader>(ShaderT::get_shader());

        RGComputePassInfo value{};
        value.shader = shader;

        RGPassInfo info{};
        info.name = name;
        info.inputs = create_inputs(members);
        info.members = members;
        info.func = func;
        info.value = value;

        m_passes.push_back(info);
    }

    //
    // Compile
    //

    std::optional<RenderGraph> compile(RenderGraphDeviceMemoryAllocator& allocator);

  private:
    // Resources

    struct RGImageDescription
    {
        ImageDescription image_desc;
        std::vector<uint8_t> data;
    };

    std::unordered_map<RGImageRef, RGImageDescription> m_transient_image_descriptions;

    struct RGExternalImageDescription
    {
        std::shared_ptr<ImageResource> resource;
        ImageResourceState input_state;
        ImageResourceState output_state;
    };
    std::unordered_map<RGImageRef, RGExternalImageDescription> m_external_image_descriptions;

    struct RGImageViewDescription
    {
        RGImageRef image_ref;
        ImageResourceViewRange range;
    };

    std::unordered_map<RGImageViewRef, RGImageViewDescription> m_transient_image_view_descriptions;

    struct RGBufferDescription
    {
        BufferDescription buffer_desc;
        std::vector<uint8_t> data;
    };

    std::unordered_map<RGBufferRef, RGBufferDescription> m_transient_buffer_descriptions;
    std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>> m_external_buffers;

    struct RGFramebufferDescription
    {
        uint32_t width, height;
        std::vector<RGImageViewRef> attachments;
    };
    std::unordered_map<RGFramebufferRef, RGFramebufferDescription> m_framebuffer_descriptions;

    // Passes

    struct RGNullPassInfo
    {
    };

    struct RGRenderPassNoPipelineInfo
    {
        RGFramebufferRef framebuffer;
    };

    struct RGRenderPassInfo
    {
        std::shared_ptr<GraphicsShader> shader;
        RGGraphicsPipelineDescription pipeline_desc;
        RGFramebufferRef framebuffer;
    };

    struct RGComputePassInfo
    {
        std::shared_ptr<ComputeShader> shader;
    };

    struct RGDebugLabelPassInfo
    {
    };

    using RGPassInfoT = std::
        variant<RGNullPassInfo, RGRenderPassNoPipelineInfo, RGRenderPassInfo, RGComputePassInfo, RGDebugLabelPassInfo>;
    struct RGPassInfo
    {
        std::string name;
        RenderGraphDependencies inputs;
        std::vector<ShaderParameterMemberInfo> members;
        std::optional<RGFramebufferRef> framebuffer;
        RGFunction func;

        RGPassInfoT value;

        template <typename T>
        bool is_type() const
        {
            static_assert(std::is_convertible_v<T, RGPassInfoT>, "T is not compatible with RGPassInfoT variant");
            return std::holds_alternative<T>(value);
        }

        template <typename T>
        T get_value() const
        {
            MIZU_ASSERT(is_type<T>(), "Value is not of the requested type");
            return std::get<T>(value);
        }
    };
    std::vector<RGPassInfo> m_passes;

    // Helpers

    RenderGraphDependencies create_inputs(const std::vector<ShaderParameterMemberInfo>& members);

    void validate_shader_declaration_members(const IShader& shader,
                                             const std::vector<ShaderParameterMemberInfo>& members);

    // Compile Helpers
    using RGImageMap = std::unordered_map<RGImageRef, std::shared_ptr<ImageResource>>;
    using RGImageViewMap = std::unordered_map<RGImageViewRef, std::shared_ptr<ImageResourceView>>;
    using RGBufferMap = std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>>;

    struct RGImageUsage
    {
        enum class Type
        {
            Sampled,
            Storage,
            Attachment,
        };

        Type type;
        size_t render_pass_idx = 0;
        RGImageRef image;
        RGImageViewRef view;
    };
    std::vector<RGImageUsage> get_image_usages(RGImageRef ref) const;

    struct RGBufferUsage
    {
        enum class Type
        {
            UniformBuffer,
            StorageBuffer
        };

        Type type;
        size_t render_pass_idx = 0;
    };
    std::vector<RGBufferUsage> get_buffer_usages(RGBufferRef ref) const;

    using ResourceMemberInfoT = std::
        variant<std::shared_ptr<ImageResourceView>, std::shared_ptr<BufferResource>, std::shared_ptr<SamplerState>>;
    struct RGResourceMemberInfo
    {
        std::string name;
        uint32_t set;
        uint32_t binding;
        ResourceMemberInfoT value;
    };

    std::shared_ptr<Framebuffer> create_framebuffer(
        const RGFramebufferDescription& framebuffer_desc,
        const RGImageMap& image_resources,
        const RGImageViewMap& image_view_resources,
        const std::unordered_map<RGImageRef, std::vector<RGImageUsage>>& image_usages,
        size_t pass_idx,
        RenderGraph& rg) const;

    std::vector<RGResourceMemberInfo> create_members(const RGPassInfo& info,
                                                     const RGImageViewMap& image_views,
                                                     const RGBufferMap& buffers,
                                                     const IShader* shader) const;

    struct RGResourceGroup
    {
        std::shared_ptr<ResourceGroup> resource_group;
        uint32_t set;
    };
    std::vector<RGResourceGroup> create_resource_groups(
        const std::vector<RGResourceMemberInfo>& members,
        std::unordered_map<size_t, RGResourceGroup>& checksum_to_resource_group,
        const std::shared_ptr<IShader>& shader) const;

    // RenderGraph passes

    void add_image_transition_pass(RenderGraph& rg,
                                   ImageResource& image,
                                   ImageResourceState old_state,
                                   ImageResourceState new_state) const;

    void add_image_transition_pass(RenderGraph& rg,
                                   ImageResource& image,
                                   ImageResourceState old_state,
                                   ImageResourceState new_state,
                                   ImageResourceViewRange range) const;

    void add_copy_to_image_pass(RenderGraph& rg,
                                std::shared_ptr<BufferResource> staging,
                                std::shared_ptr<ImageResource> image);

    void add_copy_to_buffer_pass(RenderGraph& rg,
                                 std::shared_ptr<BufferResource> staging,
                                 std::shared_ptr<BufferResource> buffer);

    void add_null_pass(RenderGraph& rg,
                       const std::string& name,
                       const std::vector<RGResourceGroup>& resource_groups,
                       const RGFunction& func) const;

    void add_render_pass_no_pipeline(RenderGraph& rg,
                                     const std::string& name,
                                     const std::shared_ptr<RenderPass>& render_pass,
                                     const std::vector<RGResourceGroup>& resource_groups,
                                     const RGFunction& func) const;

    void add_render_pass(RenderGraph& rg,
                         const std::string& name,
                         const std::shared_ptr<RenderPass>& render_pass,
                         const std::shared_ptr<GraphicsPipeline>& pipeline,
                         const std::vector<RGResourceGroup>& resource_groups,
                         const RGFunction& func) const;

    void add_compute_pass(RenderGraph& rg,
                          const std::string& name,
                          const std::shared_ptr<ComputePipeline>& pipeline,
                          const std::vector<RGResourceGroup>& resource_groups,
                          const RGFunction& func) const;
};

} // namespace Mizu
