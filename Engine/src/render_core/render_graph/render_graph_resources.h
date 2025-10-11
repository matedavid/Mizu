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

    std::shared_ptr<ImageResourceView> get_image_view(RGImageViewRef ref) const
    {
        const auto it = m_image_view_resources.find(ref);
        MIZU_ASSERT(
            it != m_image_view_resources.end(),
            "ImageView with id '{}' is not registered in pass",
            static_cast<UUID::Type>(ref));
        return it->second;
    }

    std::shared_ptr<BufferResource> get_buffer(RGBufferRef ref) const
    {
        const auto it = m_buffer_resources.find(ref);
        MIZU_ASSERT(
            it != m_buffer_resources.end(),
            "Buffer with id '{}' is not registered in pass",
            static_cast<UUID::Type>(ref));
        return it->second;
    }

  private:
    void set_framebuffer(std::shared_ptr<Framebuffer> framebuffer) { m_framebuffer = std::move(framebuffer); }
    void set_resource_group_map(RGResourceGroupMap* resource_group_map) { m_resource_group_map = resource_group_map; }

    void add_image_view(RGImageViewRef ref, std::shared_ptr<ImageResourceView> image_view)
    {
        m_image_view_resources.insert({ref, image_view});
    }

    void add_buffer(RGBufferRef ref, std::shared_ptr<BufferResource> buffer)
    {
        m_buffer_resources.insert({ref, buffer});
    }

    std::shared_ptr<Framebuffer> m_framebuffer{nullptr};
    RGResourceGroupMap* m_resource_group_map;

    RGImageViewMap m_image_view_resources;
    RGBufferMap m_buffer_resources;

    friend class RenderGraphBuilder;
};

struct RGLayoutResourceImageView
{
    RGImageViewRef value;
    ShaderImageProperty::Type type;
};

struct RGLayoutResourceBuffer
{
    RGBufferRef value;
    ShaderBufferProperty::Type type;
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
    RGLayoutResourceImageView,
    RGLayoutResourceBuffer,
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
    RGResourceGroupLayout& add_resource(
        uint32_t binding,
        RGImageViewRef resource,
        ShaderType stage,
        ShaderImageProperty::Type type)
    {
        RGLayoutResourceImageView value{};
        value.value = resource;
        value.type = type;

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = value;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGUniformBufferRef resource, ShaderType stage)
    {
        return add_resource(binding, resource, stage, ShaderBufferProperty::Type::Uniform);
    }

    RGResourceGroupLayout& add_resource(uint32_t binding, RGStorageBufferRef resource, ShaderType stage)
    {
        return add_resource(binding, resource, stage, ShaderBufferProperty::Type::Storage);
    }

    RGResourceGroupLayout& add_resource(
        uint32_t binding,
        RGBufferRef resource,
        ShaderType stage,
        ShaderBufferProperty::Type type)
    {
        RGLayoutResourceBuffer value{};
        value.value = resource;
        value.type = type;

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
