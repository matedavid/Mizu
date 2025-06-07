#pragma once

#include <memory>
#include <unordered_map>

#include "render_core/rhi/resource_group.h"
#include "render_core/shader/shader_parameters.h"

#include "utility/assert.h"

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
        const auto it = m_resource_group_map.find(ref);
        MIZU_ASSERT(it != m_resource_group_map.end(),
                    "ResourceGroup with id {} is not registered",
                    static_cast<UUID::Type>(ref));

        return it->second;
    }

  private:
    void set_framebuffer(std::shared_ptr<Framebuffer> framebuffer) { m_framebuffer = std::move(framebuffer); }
    void set_resource_group_map(
        std::unordered_map<RGResourceGroupRef, std::shared_ptr<ResourceGroup>> resource_group_map)
    {
        m_resource_group_map = std::move(resource_group_map);
    }

    std::shared_ptr<Framebuffer> m_framebuffer{nullptr};
    std::unordered_map<RGResourceGroupRef, std::shared_ptr<ResourceGroup>> m_resource_group_map;

    friend class RenderGraphBuilder;
};

class RGResourceGroupLayoutResource
{
  public:
    uint32_t binding;
    ShaderParameterMemberT value;
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
};

class RGResourceGroupLayout
{
  public:
    template <typename T>
    RGResourceGroupLayout& add_resource(uint32_t binding, T resource, ShaderType stage)
    {
        static_assert(is_in_variant<T, ShaderParameterMemberT>::value, "Resource type is not allowed");

        RGResourceGroupLayoutResource item{};
        item.binding = binding;
        item.value = resource;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    const std::vector<RGResourceGroupLayoutResource>& get_resources() const { return m_resources; }

  private:
    std::vector<RGResourceGroupLayoutResource> m_resources;
};

} // namespace Mizu
