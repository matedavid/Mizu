#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "render_core/resources/buffers.h"
#include "render_core/resources/cubemap.h"
#include "render_core/resources/texture.h"

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/resource_view.h"

#include "render_core/render_graph/render_graph.h"
#include "render_core/render_graph/render_graph_resources.h"
#include "render_core/render_graph/render_graph_types.h"

#include "render_core/shader/shader_declaration.h"

namespace Mizu
{

template <typename T>
concept IsValidParametersType = requires(T t) {
    { t.get_members({}) } -> std::same_as<std::vector<ShaderParameterMemberInfo>>;
};

template <typename T>
concept IsContainer = requires(const T& c) {
    { c.data() } -> std::convertible_to<const typename T::value_type*>;
    { c.size() } -> std::convertible_to<size_t>;
    { c.empty() } -> std::convertible_to<bool>;
};

class RGBuilderPass
{
  public:
    template <typename ParamsT>
        requires IsValidParametersType<ParamsT>
    RGBuilderPass(std::string name, const ParamsT& params, RGPassHint hint, RGFunction func)
        : m_name(std::move(name))
        , m_hint(hint)
        , m_func(std::move(func))
    {
        for (const ShaderParameterMemberInfo& member : ParamsT::get_members(params))
        {
            if (member.mem_name == "framebuffer"
                && member.mem_type == ShaderParameterMemberType::RGFramebufferAttachments)
            {
                m_framebuffer = std::get<RGFramebufferAttachments>(member.value);
                continue;
            }

            switch (member.mem_type)
            {
            case ShaderParameterMemberType::RGSampledImageView:
            case ShaderParameterMemberType::RGStorageImageView:
                m_image_view_members.push_back(member);
                break;
            case ShaderParameterMemberType::RGUniformBuffer:
            case ShaderParameterMemberType::RGStorageBuffer:
                m_buffer_members.push_back(member);
                break;
            case ShaderParameterMemberType::SamplerState:
                m_sampler_state_members.push_back(member);
                break;
            case ShaderParameterMemberType::RGAccelerationStructure:
                m_acceleration_structure_members.push_back(member);
                break;
            case ShaderParameterMemberType::RGFramebufferAttachments:
                // Already treated before
                break;
            default:
                MIZU_UNREACHABLE("ShaderParameterMemberType is not valid");
            }
        }
    }

    const std::vector<ShaderParameterMemberInfo>& get_image_view_members() const { return m_image_view_members; }
    const std::vector<ShaderParameterMemberInfo>& get_buffer_members() const { return m_buffer_members; }
    const std::vector<ShaderParameterMemberInfo>& get_sampler_state_members() const { return m_sampler_state_members; }
    const std::vector<ShaderParameterMemberInfo>& get_acceleration_structure_members() const
    {
        return m_acceleration_structure_members;
    }

    bool has_framebuffer() const { return m_framebuffer.has_value(); }

    const RGFramebufferAttachments& get_framebuffer() const
    {
        MIZU_ASSERT(has_framebuffer(), "RGBuilderPass does not have a framebuffer");
        return *m_framebuffer;
    }

    const std::string& get_name() const { return m_name; }
    RGPassHint get_hint() const { return m_hint; }
    const RGFunction& get_function() const { return m_func; }

  private:
    std::string m_name;
    RGPassHint m_hint;
    RGFunction m_func;

    std::optional<RGFramebufferAttachments> m_framebuffer;

    std::vector<ShaderParameterMemberInfo> m_image_view_members;
    std::vector<ShaderParameterMemberInfo> m_buffer_members;
    std::vector<ShaderParameterMemberInfo> m_sampler_state_members;
    std::vector<ShaderParameterMemberInfo> m_acceleration_structure_members;
};

struct RenderGraphBuilderMemory
{
    AliasedDeviceMemoryAllocator& allocator;
    AliasedDeviceMemoryAllocator& staging_allocator;

    RenderGraphBuilderMemory(AliasedDeviceMemoryAllocator& allocator_, AliasedDeviceMemoryAllocator& staging_allocator_)
        : allocator(allocator_)
        , staging_allocator(staging_allocator_)
    {
    }
};

class RenderGraphBuilder
{
  public:
    RenderGraphBuilder() = default;

    //
    // Resources
    //

    template <typename TextureT>
    RGTextureRef create_texture(
        decltype(TextureT::Description::dimensions) dimensions,
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
    RGTextureRef create_texture(
        decltype(TextureT::Description::dimensions) dimensions,
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
        desc.data = std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()),
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
        desc.data = std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(&data), reinterpret_cast<const uint8_t*>(&data) + sizeof(T));

        auto id = RGUniformBufferRef();
        m_transient_buffer_descriptions.insert({id, desc});

        return id;
    }

    RGUniformBufferRef register_external_buffer(const UniformBuffer& ubo);

    RGStorageBufferRef create_storage_buffer(uint64_t size, std::string name = "");
    RGStorageBufferRef create_storage_buffer(BufferDescription buffer_desc);

    template <typename ContainerT>
        requires IsContainer<ContainerT>
    RGStorageBufferRef create_storage_buffer(const ContainerT& data, std::string name = "")
    {
        BufferDescription buffer_desc =
            StorageBuffer::get_buffer_description(sizeof(typename ContainerT::value_type) * data.size(), name);

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
            desc.data = std::vector<uint8_t>(
                reinterpret_cast<const uint8_t*>(data.data()),
                reinterpret_cast<const uint8_t*>(data.data()) + buffer_desc.size);
        }

        auto id = RGStorageBufferRef();
        m_transient_buffer_descriptions.insert({id, desc});

        return id;
    }

    RGStorageBufferRef register_external_buffer(const StorageBuffer& ssbo);

    RGAccelerationStructureRef register_external_acceleration_structure(std::shared_ptr<AccelerationStructure> as);

    RGResourceGroupRef create_resource_group(const RGResourceGroupLayout& layout);

    void begin_gpu_marker(std::string name);
    void end_gpu_marker();

    //
    // Passes
    //

    template <typename ParamsT>
        requires IsValidParametersType<ParamsT>
    void add_pass(std::string name, ParamsT params, RGPassHint hint, RGFunction func)
    {
        RGBuilderPass pass(std::move(name), std::move(params), hint, std::move(func));
        m_passes.push_back(pass);
    }

    //
    // Compile
    //

    void compile(RenderGraph& rg, const RenderGraphBuilderMemory& memory);

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

    std::unordered_map<RGAccelerationStructureRef, std::shared_ptr<AccelerationStructure>> m_external_as;

    std::unordered_map<RGResourceGroupRef, RGResourceGroupLayout> m_resource_group_descriptions;
    std::unordered_map<size_t, RGResourceGroupRef> m_resource_group_cache;

    // Passes

    std::vector<RGBuilderPass> m_passes;

    // Helpers

    bool image_view_references_image(RGImageViewRef view_ref, RGImageRef image_ref) const;

    // Compile Helpers

    using RGImageMap = std::unordered_map<RGImageRef, std::shared_ptr<ImageResource>>;
    using RGImageViewMap = std::unordered_map<RGImageViewRef, std::shared_ptr<ImageResourceView>>;
    using RGBufferMap = std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>>;
    using RGAccelerationStructureMap =
        std::unordered_map<RGAccelerationStructureRef, std::shared_ptr<AccelerationStructure>>;
    using RGSResourceGroupMap = std::unordered_map<RGResourceGroupRef, std::shared_ptr<ResourceGroup>>;

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

    Framebuffer::Attachment create_framebuffer_attachment(
        RenderGraph& rg,
        const RGImageViewRef& attachment_view,
        const RGImageMap& image_resources,
        const RGImageViewMap& image_view_resources,
        const std::unordered_map<RGImageRef, std::vector<RGImageUsage>>& image_usages,
        size_t pass_idx);

    // RenderGraph passes

    void add_image_transition_pass(
        RenderGraph& rg,
        ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state) const;

    void add_image_transition_pass(
        RenderGraph& rg,
        ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state,
        ImageResourceViewRange range) const;

    void add_copy_to_image_pass(
        RenderGraph& rg,
        std::shared_ptr<BufferResource> staging,
        std::shared_ptr<ImageResource> image);

    void add_copy_to_buffer_pass(
        RenderGraph& rg,
        std::shared_ptr<BufferResource> staging,
        std::shared_ptr<BufferResource> buffer);

    void add_pass(RenderGraph& rg, const std::string& name, const RGPassResources& resources, const RGFunction& func)
        const;
};

} // namespace Mizu
