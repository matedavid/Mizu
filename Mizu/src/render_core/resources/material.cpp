#include "material.h"

#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

Material::Material(std::shared_ptr<GraphicsShader> shader) : m_shader(std::move(shader)) {}

void Material::set(const std::string& name, std::shared_ptr<ImageResource> resource)
{
    const std::optional<ShaderProperty>& property = m_shader->get_property(name);
    MIZU_ASSERT(property.has_value(), "Shader does not contain image property named {}", name);

    MaterialData data{};
    data.property = *property;
    data.value = resource;

    m_resources.push_back(data);
}

void Material::set(const std::string& name, std::shared_ptr<BufferResource> resource)
{
    const std::optional<ShaderProperty>& property = m_shader->get_property(name);
    MIZU_ASSERT(property.has_value(), "Shader does not contain buffer property named {}", name);

    MaterialData data{};
    data.property = *property;
    data.value = resource;

    m_resources.push_back(data);
}

bool Material::bake()
{
    // Find max binding set
    uint32_t max_binding_set = 0;

    for (const MaterialData& data : m_resources)
    {
        max_binding_set = std::max(max_binding_set, data.property.binding_info.set);
    }

    // Create resource groups
    std::vector<std::shared_ptr<ResourceGroup>> set_to_resource_group(max_binding_set + 1, nullptr);

    for (const MaterialData& data : m_resources)
    {
        const ShaderProperty& property = data.property;

        if (set_to_resource_group[property.binding_info.set] == nullptr)
        {
            set_to_resource_group[property.binding_info.set] = ResourceGroup::create();
        }

        std::visit(
            [&](auto&& value) { set_to_resource_group[property.binding_info.set]->add_resource(property.name, value); },
            data.value);
    }

    // Bake resource groups
    for (uint32_t i = 0; i < set_to_resource_group.size(); ++i)
    {
        auto& resource_group = set_to_resource_group[i];
        if (resource_group == nullptr)
            continue;

        MIZU_ASSERT(resource_group->bake(*m_shader, i), "Could not bake material resource group with set {}", i);
        m_resource_groups.push_back(resource_group);
    }

    m_is_baked = true;
    return true;
}

} // namespace Mizu
