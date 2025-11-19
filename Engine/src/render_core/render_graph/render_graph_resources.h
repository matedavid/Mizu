#pragma once

#include <memory>
#include <unordered_map>

#include "base/debug/assert.h"

#include "render_core/render_graph/render_graph_shader_parameters.h"
#include "render_core/render_graph/render_graph_types.h"
#include "render_core/rhi/resource_group.h"

namespace Mizu
{

// Forward declarations
class Framebuffer;
class ResourceGroup;

class RGPassResources
{
  public:
    std::shared_ptr<Framebuffer> get_framebuffer() const
    {
        MIZU_ASSERT(m_framebuffer != nullptr, "RGPassResources does not contain a framebuffer");
        return m_framebuffer;
    }

    std::shared_ptr<ResourceGroup> get_resource_group(RGResourceGroupRef ref) const
    {
        const auto it = m_resource_group_map->find(ref);
        MIZU_ASSERT(
            it != m_resource_group_map->end(),
            "ResourceGroup with id {} is not registered",
            static_cast<UUID::Type>(ref));

        return it->second;
    }

  private:
    void set_framebuffer(std::shared_ptr<Framebuffer> framebuffer) { m_framebuffer = std::move(framebuffer); }
    void set_resource_group_map(RGResourceGroupMap* resource_group_map) { m_resource_group_map = resource_group_map; }

    void add_image(RGImageRef ref, std::shared_ptr<ImageResource> image) { m_images_map.insert({ref, image}); }
    void add_buffer(RGBufferRef ref, std::shared_ptr<BufferResource> buffer) { m_buffers_map.insert({ref, buffer}); }

    void add_texture_srv(RGTextureSrvRef ref, std::shared_ptr<ShaderResourceView> view)
    {
        RGTextureView texture_view{};
        texture_view.view = ref;
        texture_view.value = view;

        m_texture_views.insert({ref, texture_view});
    }

    void add_texture_uav(RGTextureUavRef ref, std::shared_ptr<UnorderedAccessView> view)
    {
        RGTextureView texture_view{};
        texture_view.view = ref;
        texture_view.value = view;

        m_texture_views.insert({ref, texture_view});
    }

    void add_texture_rtv(RGTextureRtvRef ref, std::shared_ptr<RenderTargetView> view)
    {
        RGTextureView texture_view{};
        texture_view.view = ref;
        texture_view.value = view;

        m_texture_views.insert({ref, texture_view});
    }

    void add_buffer_srv(RGBufferSrvRef ref, std::shared_ptr<ShaderResourceView> view)
    {
        RGBufferView buffer_view{};
        buffer_view.view = ref;
        buffer_view.value = view;

        m_buffer_views.insert({ref, buffer_view});
    }

    void add_buffer_uav(RGBufferUavRef ref, std::shared_ptr<UnorderedAccessView> view)
    {
        RGBufferView buffer_view{};
        buffer_view.view = ref;
        buffer_view.value = view;

        m_buffer_views.insert({ref, buffer_view});
    }

    void add_buffer_cbv(RGBufferCbvRef ref, std::shared_ptr<ConstantBufferView> view)
    {
        RGBufferView buffer_view{};
        buffer_view.view = ref;
        buffer_view.value = view;

        m_buffer_views.insert({ref, buffer_view});
    }

    std::shared_ptr<Framebuffer> m_framebuffer{nullptr};
    RGResourceGroupMap* m_resource_group_map;

    using RGTextureViewT = std::variant<RGTextureSrvRef, RGTextureUavRef, RGTextureRtvRef>;
    using RGTextureViewValuesT = std::variant<
        std::shared_ptr<ShaderResourceView>,
        std::shared_ptr<UnorderedAccessView>,
        std::shared_ptr<RenderTargetView>>;

    struct RGTextureView
    {
        RGTextureViewT view;
        RGTextureViewValuesT value;
    };

    using RGBufferViewT = std::variant<RGBufferSrvRef, RGBufferUavRef, RGBufferCbvRef>;
    using RGBufferViewValuesT = std::variant<
        std::shared_ptr<ShaderResourceView>,
        std::shared_ptr<UnorderedAccessView>,
        std::shared_ptr<ConstantBufferView>>;

    struct RGBufferView
    {
        RGBufferViewT view;
        RGBufferViewValuesT value;
    };

    RGImageMap m_images_map;
    RGBufferMap m_buffers_map;

    std::unordered_map<RGTextureViewT, RGTextureView> m_texture_views;
    std::unordered_map<RGBufferViewT, RGBufferView> m_buffer_views;

    friend class RenderGraphBuilder;
};

struct RGLayoutResourceTextureSrv
{
    RGTextureSrvRef value;
};

struct RGLayoutResourceTextureUav
{
    RGTextureUavRef value;
};

struct RGLayoutResourceBufferSrv
{
    RGBufferSrvRef value;
};

struct RGLayoutResourceBufferUav
{
    RGBufferUavRef value;
};

struct RGLayoutResourceBufferCbv
{
    RGBufferCbvRef value;
};

struct RGLayoutResourceSamplerState
{
    std::shared_ptr<SamplerState> value;
};

struct RGLayoutResourceAccelerationStructure
{
    RGAccelerationStructureRef value;
};

using RGLayoutResourceValueT = std::variant<
    RGLayoutResourceTextureSrv,
    RGLayoutResourceTextureUav,
    RGLayoutResourceBufferSrv,
    RGLayoutResourceBufferUav,
    RGLayoutResourceBufferCbv,
    RGLayoutResourceSamplerState,
    RGLayoutResourceAccelerationStructure>;

class RGResourceGroupLayoutResource
{
  public:
    uint32_t binding;
    RGLayoutResourceValueT value;
    ShaderType stage;

    template <typename T>
    bool is_type() const
    {
        return std::holds_alternative<T>(value);
    }

    template <typename T>
    T as_type() const
    {
        MIZU_ASSERT(is_type<T>(), "Resource value is not of type {}", typeid(T).name());
        return std::get<T>(value);
    }

    size_t hash() const
    {
        const size_t value_hash = std::visit(
            [&](auto&& v) -> size_t {
                using T = std::decay_t<decltype(v)>;

                return std::hash<decltype(T::value)>()(v.value);
            },
            value);

        return std::hash<uint32_t>()(binding) ^ value_hash ^ std::hash<ShaderType>()(stage);
    }
};

class RGResourceGroupLayout
{
  public:
    RGResourceGroupLayout& add_resource(uint32_t binding, RGTextureSrvRef resource, ShaderType stage)
    {
        RGLayoutResourceTextureSrv value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGTextureUavRef resource, ShaderType stage)
    {
        RGLayoutResourceTextureUav value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGBufferSrvRef resource, ShaderType stage)
    {
        RGLayoutResourceBufferSrv value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGBufferUavRef resource, ShaderType stage)
    {
        RGLayoutResourceBufferUav value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGBufferCbvRef resource, ShaderType stage)
    {
        RGLayoutResourceBufferCbv value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, std::shared_ptr<SamplerState> resource, ShaderType stage)
    {
        RGLayoutResourceSamplerState value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGAccelerationStructureRef resource, ShaderType stage)
    {
        RGLayoutResourceAccelerationStructure value{};
        value.value = resource;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    size_t hash() const
    {
        size_t hash = 0;
        for (const RGResourceGroupLayoutResource& resource : m_resources)
        {
            hash ^= resource.hash();
        }

        return hash;
    }

    const std::vector<RGResourceGroupLayoutResource>& get_resources() const { return m_resources; }

  private:
    std::vector<RGResourceGroupLayoutResource> m_resources;
};

} // namespace Mizu
