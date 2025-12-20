#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/resource_view.h"

#include "renderer/render_graph/render_graph.h"
#include "renderer/render_graph/render_graph_resources.h"
#include "renderer/render_graph/render_graph_types.h"

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
            case ShaderParameterMemberType::RGTextureSrv:
            case ShaderParameterMemberType::RGTextureUav:
                m_texture_view_members.push_back(member);
                break;
            case ShaderParameterMemberType::RGBufferSrv:
            case ShaderParameterMemberType::RGBufferUav:
            case ShaderParameterMemberType::RGBufferCbv:
                m_buffer_view_members.push_back(member);
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

    const std::vector<ShaderParameterMemberInfo>& get_texture_view_members() const { return m_texture_view_members; }
    const std::vector<ShaderParameterMemberInfo>& get_buffer_view_members() const { return m_buffer_view_members; }
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

    std::vector<ShaderParameterMemberInfo> m_texture_view_members;
    std::vector<ShaderParameterMemberInfo> m_buffer_view_members;
    std::vector<ShaderParameterMemberInfo> m_sampler_state_members;
    std::vector<ShaderParameterMemberInfo> m_acceleration_structure_members;
};

struct RenderGraphBuilderMemory
{
    AliasedDeviceMemoryAllocator& transient_allocator;
    AliasedDeviceMemoryAllocator& host_allocator;

    RenderGraphBuilderMemory(
        AliasedDeviceMemoryAllocator& transient_allocator_,
        AliasedDeviceMemoryAllocator& host_allocator_)
        : transient_allocator(transient_allocator_)
        , host_allocator(host_allocator_)
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

    RGImageRef create_image(ImageDescription desc, std::vector<uint8_t> data);
    RGImageRef create_image(ImageDescription desc);

    RGImageRef create_texture(glm::uvec2 dimensions, ImageFormat format, std::string name = "")
    {
        ImageDescription image_desc{};
        image_desc.width = dimensions.x;
        image_desc.height = dimensions.y;
        image_desc.type = ImageType::Image2D;
        image_desc.format = format;
        image_desc.name = std::move(name);

        return create_image(image_desc);
    }

    RGImageRef create_cubemap(glm::uvec2 dimensions, ImageFormat format, std::string name = "");

    RGBufferRef create_buffer(BufferDescription desc, std::vector<uint8_t> data);
    RGBufferRef create_buffer(BufferDescription desc);
    RGBufferRef create_buffer(size_t size, size_t stride, std::string name = "");

    template <typename T>
    RGBufferRef create_constant_buffer(const T& data, std::string name = "")
    {
        BufferDescription buffer_desc{};
        buffer_desc.size = sizeof(T);
        buffer_desc.usage = BufferUsageBits::ConstantBuffer;
        buffer_desc.name = std::move(name);

        if (buffer_desc.size == 0)
        {
            // Cannot create empty buffer, if size is zero set to 1
            // TODO: Rethink this approach
            buffer_desc.size = 1;
        }

        const std::vector<uint8_t> uint_data = std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(&data), reinterpret_cast<const uint8_t*>(&data) + sizeof(T));

        return create_buffer(buffer_desc, uint_data);
    }

    template <typename T>
    RGBufferRef create_structured_buffer(uint64_t number, std::string name = "")
    {
        BufferDescription buffer_desc{};
        buffer_desc.size = number * sizeof(T);
        buffer_desc.stride = sizeof(T);
        buffer_desc.usage = BufferUsageBits::UnorderedAccess;
        buffer_desc.name = std::move(name);

        return create_buffer(buffer_desc);
    }

    template <typename T, typename ContainerT>
        requires IsContainer<ContainerT>
    RGBufferRef create_structured_buffer(const ContainerT& data, std::string name = "")
    {
        BufferDescription buffer_desc{};
        buffer_desc.size = data.size() * sizeof(T);
        buffer_desc.stride = sizeof(T);
        buffer_desc.usage = BufferUsageBits::UnorderedAccess;
        buffer_desc.name = std::move(name);

        const std::vector<uint8_t> uint_data = std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()),
            reinterpret_cast<const uint8_t*>(data.data()) + buffer_desc.size);

        return create_buffer(buffer_desc, uint_data);
    }

    RGImageRef register_external_texture(std::shared_ptr<ImageResource> texture, RGExternalTextureParams params)
    {
        RGExternalImageDescription desc{};
        desc.resource = texture;
        desc.input_state = params.input_state;
        desc.output_state = params.output_state;

        auto id = RGImageRef();
        m_external_image_descriptions.insert({id, desc});

        return id;
    }

    RGBufferRef register_external_constant_buffer(std::shared_ptr<BufferResource> cbo, RGExternalBufferParams params);
    RGBufferRef register_external_structured_buffer(std::shared_ptr<BufferResource> sbo, RGExternalBufferParams params);

    RGAccelerationStructureRef register_external_acceleration_structure(std::shared_ptr<AccelerationStructure> as);

    RGResourceGroupRef create_resource_group(const RGResourceGroupLayout& layout);

    RGTextureSrvRef create_texture_srv(RGImageRef image_ref, ImageFormat format, ImageResourceViewRange range = {});
    RGTextureSrvRef create_texture_srv(RGImageRef image_ref, ImageResourceViewRange range = {});
    RGTextureUavRef create_texture_uav(RGImageRef image_ref, ImageFormat format, ImageResourceViewRange range = {});
    RGTextureUavRef create_texture_uav(RGImageRef image_ref, ImageResourceViewRange range = {});
    RGTextureRtvRef create_texture_rtv(RGImageRef image_ref, ImageFormat format, ImageResourceViewRange range = {});
    RGTextureRtvRef create_texture_rtv(RGImageRef image_ref, ImageResourceViewRange range = {});

    RGBufferSrvRef create_buffer_srv(RGBufferRef buffer_ref);
    RGBufferSrvRef create_buffer_uav(RGBufferRef buffer_ref);
    RGBufferCbvRef create_buffer_cbv(RGBufferRef buffer_ref);

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
    enum class RGResourceUsageType
    {
        Read,
        ReadWrite,
        Attachment,
        ConstantBuffer,
    };

    enum class RGResourceType
    {
        Image,
        Buffer,
        AccelerationStructure,
    };

    // Resources

    struct RGImageDescription
    {
        ImageDescription image_desc;
        std::vector<uint8_t> data;
    };

    struct RGBufferDescription
    {
        BufferDescription buffer_desc;
        std::vector<uint8_t> data;
    };

    struct RGExternalImageDescription
    {
        std::shared_ptr<ImageResource> resource;
        ImageResourceState input_state;
        ImageResourceState output_state;
    };

    struct RGExternalBufferDescription
    {
        std::shared_ptr<BufferResource> resource;
        BufferResourceState input_state;
        BufferResourceState output_state;
    };

    struct RGTextureViewDescription
    {
        RGImageRef image_ref;
        RGResourceUsageType usage;
        ImageFormat format;
        ImageResourceViewRange range;
    };

    struct RGBufferViewDescription
    {
        RGBufferRef buffer_ref;
        RGResourceUsageType usage;
    };

    std::unordered_map<RGImageRef, RGImageDescription> m_transient_image_descriptions;
    std::unordered_map<RGBufferRef, RGBufferDescription> m_transient_buffer_descriptions;

    std::unordered_map<RGImageRef, RGExternalImageDescription> m_external_image_descriptions;
    std::unordered_map<RGBufferRef, RGExternalBufferDescription> m_external_buffer_descriptions;

    std::unordered_map<RGTextureViewRef, RGTextureViewDescription> m_transient_texture_view_descriptions;
    std::unordered_map<RGBufferViewRef, RGBufferViewDescription> m_transient_buffer_view_descriptions;

    std::unordered_map<RGAccelerationStructureRef, std::shared_ptr<AccelerationStructure>> m_external_as;

    std::unordered_map<RGResourceGroupRef, RGResourceGroupLayout> m_resource_group_descriptions;
    std::unordered_map<size_t, RGResourceGroupRef> m_resource_group_cache;

    // Passes

    std::vector<RGBuilderPass> m_passes;

    // Helpers

    struct RGResourceUsage
    {
        RGResourceRef resource;
        size_t pass_idx;
        RGResourceUsageType usage;
        RGResourceType resource_type;

        RGResourceUsage() = default;
        RGResourceUsage(
            RGResourceRef resource_,
            size_t pass_idx_,
            RGResourceUsageType usage_,
            RGResourceType resource_type_)
            : resource(resource_)
            , pass_idx(pass_idx_)
            , usage(usage_)
            , resource_type(resource_type_)
        {
        }
    };

    struct RGTextureViewsWrapper
    {
        void add(RGTextureSrvRef ref, std::shared_ptr<ShaderResourceView> view) { m_texture_srvs.insert({ref, view}); }
        void add(RGTextureUavRef ref, std::shared_ptr<UnorderedAccessView> view) { m_texture_uavs.insert({ref, view}); }
        void add(RGTextureRtvRef ref, std::shared_ptr<RenderTargetView> view) { m_texture_rtvs.insert({ref, view}); }

        std::shared_ptr<ShaderResourceView> get(RGTextureSrvRef ref) const
        {
            const auto it = m_texture_srvs.find(ref);
            MIZU_ASSERT(it != m_texture_srvs.end(), "Texture Srv with id {} not found", static_cast<UUID::Type>(ref));
            return it->second;
        }

        std::shared_ptr<UnorderedAccessView> get(RGTextureUavRef ref) const
        {
            const auto it = m_texture_uavs.find(ref);
            MIZU_ASSERT(it != m_texture_uavs.end(), "Texture Uav with id {} not found", static_cast<UUID::Type>(ref));
            return it->second;
        }

        std::shared_ptr<RenderTargetView> get(RGTextureRtvRef ref) const
        {
            const auto it = m_texture_rtvs.find(ref);
            MIZU_ASSERT(it != m_texture_rtvs.end(), "Texture Rtv with id {} not found", static_cast<UUID::Type>(ref));
            return it->second;
        }

      private:
        std::unordered_map<RGTextureSrvRef, std::shared_ptr<ShaderResourceView>> m_texture_srvs;
        std::unordered_map<RGTextureUavRef, std::shared_ptr<UnorderedAccessView>> m_texture_uavs;
        std::unordered_map<RGTextureRtvRef, std::shared_ptr<RenderTargetView>> m_texture_rtvs;
    };

    struct RGBufferViewsWrapper
    {
        void add(RGBufferSrvRef ref, std::shared_ptr<ShaderResourceView> view) { m_buffer_srvs.insert({ref, view}); }
        void add(RGBufferUavRef ref, std::shared_ptr<UnorderedAccessView> view) { m_buffer_uavs.insert({ref, view}); }
        void add(RGBufferCbvRef ref, std::shared_ptr<ConstantBufferView> view) { m_buffer_cbvs.insert({ref, view}); }

        std::shared_ptr<ShaderResourceView> get(RGBufferSrvRef ref) const
        {
            const auto it = m_buffer_srvs.find(ref);
            MIZU_ASSERT(it != m_buffer_srvs.end(), "Buffer Srv with id {} not found", static_cast<UUID::Type>(ref));
            return it->second;
        }

        std::shared_ptr<UnorderedAccessView> get(RGBufferUavRef ref) const
        {
            const auto it = m_buffer_uavs.find(ref);
            MIZU_ASSERT(it != m_buffer_uavs.end(), "Buffer Uav with id {} not found", static_cast<UUID::Type>(ref));
            return it->second;
        }

        std::shared_ptr<ConstantBufferView> get(RGBufferCbvRef ref) const
        {
            const auto it = m_buffer_cbvs.find(ref);
            MIZU_ASSERT(it != m_buffer_cbvs.end(), "Buffer Cbv with id {} not found", static_cast<UUID::Type>(ref));
            return it->second;
        }

      private:
        std::unordered_map<RGBufferSrvRef, std::shared_ptr<ShaderResourceView>> m_buffer_srvs;
        std::unordered_map<RGBufferUavRef, std::shared_ptr<UnorderedAccessView>> m_buffer_uavs;
        std::unordered_map<RGBufferCbvRef, std::shared_ptr<ConstantBufferView>> m_buffer_cbvs;
    };

    using RGResourceUsageMap = std::unordered_map<RGResourceRef, std::vector<RGResourceUsage>>;

    void get_image_usages(RGImageRef ref, RGResourceUsageMap& usages_map) const;
    void get_buffer_usages(RGBufferRef ref, RGResourceUsageMap& usages_map) const;

    FramebufferAttachment create_framebuffer_attachment(
        RenderGraph& rg,
        const RGTextureRtvRef& rtv_ref,
        const RGImageMap& image_resources,
        const RGTextureViewsWrapper& texture_views,
        const RGResourceUsageMap& resource_usages,
        size_t pass_idx);

    ImageFormat get_image_format(RGImageRef ref) const;
    const RGTextureViewDescription& get_texture_view_description(RGTextureViewRef ref) const;

    bool texture_view_references_image(RGTextureViewRef view_ref, RGImageRef image_ref) const;
    RGImageRef get_image_from_texture_view(RGTextureViewRef view_ref) const;

    bool buffer_view_references_buffer(RGBufferViewRef view_ref, RGBufferRef buffer_ref) const;
    RGBufferRef get_buffer_from_buffer_view(RGBufferViewRef view_ref) const;

    // RenderGraph passes

    void add_image_transition_pass(
        RenderGraph& rg,
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state) const;

    void add_image_transition_pass(
        RenderGraph& rg,
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state,
        ImageResourceViewRange range) const;

    void add_buffer_transition_pass(
        RenderGraph& rg,
        const BufferResource& buffer,
        BufferResourceState old_state,
        BufferResourceState new_state);

    void add_copy_to_image_pass(RenderGraph& rg, const BufferResource& staging, const ImageResource& image) const;

    void add_copy_to_buffer_pass(RenderGraph& rg, const BufferResource& staging, const BufferResource& buffer) const;

    void add_pass(RenderGraph& rg, const std::string& name, const RGPassResources& resources, const RGFunction& func)
        const;
};

} // namespace Mizu
