#include "material.h"

#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

Material::Material(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader)
    : m_vertex_shader(std::move(vertex_shader))
    , m_fragment_shader(std::move(fragment_shader))
{
    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(*m_vertex_shader);
    m_shader_group.add_shader(*m_fragment_shader);
}

void Material::set(const std::string& name, std::shared_ptr<ImageResourceView> resource)
{
    MaterialData data{};
    data.property = m_shader_group.get_property_info(name);
    data.value = resource;

    m_resources.push_back(data);
}

void Material::set(const std::string& name, std::shared_ptr<BufferResource> resource)
{
    MaterialData data{};
    data.property = m_shader_group.get_property_info(name);
    data.value = resource;

    m_resources.push_back(data);
}

void Material::set(const std::string& name, std::shared_ptr<SamplerState> resource)
{
    MaterialData data{};
    data.property = m_shader_group.get_property_info(name);
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
    std::vector<ResourceGroupLayout> set_to_resource_group_layout(max_binding_set + 1);

    for (const MaterialData& data : m_resources)
    {
        const ShaderProperty& property = data.property;
        const ShaderType stage_bits = m_shader_group.get_resource_stage_bits(property.name);

        ResourceGroupLayout& layout = set_to_resource_group_layout[property.binding_info.set];

        if (std::holds_alternative<std::shared_ptr<ImageResourceView>>(data.value))
        {
            const ShaderImageProperty& image_property = std::get<ShaderImageProperty>(property.value);

            const auto& value = std::get<std::shared_ptr<ImageResourceView>>(data.value);
            layout.add_resource(property.binding_info.binding, value, stage_bits, image_property.type);
        }
        else if (std::holds_alternative<std::shared_ptr<BufferResource>>(data.value))
        {
            const ShaderBufferProperty& buffer_property = std::get<ShaderBufferProperty>(property.value);

            const auto& value = std::get<std::shared_ptr<BufferResource>>(data.value);
            layout.add_resource(property.binding_info.binding, value, stage_bits, buffer_property.type);
        }
        else if (std::holds_alternative<std::shared_ptr<SamplerState>>(data.value))
        {
            const auto& value = std::get<std::shared_ptr<SamplerState>>(data.value);
            layout.add_resource(property.binding_info.binding, value, stage_bits);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid material data or not implemented");
        }
    }

    // Bake resource groups
    for (uint32_t set = 0; set < set_to_resource_group_layout.size(); ++set)
    {
        const ResourceGroupLayout& layout = set_to_resource_group_layout[set];
        if (layout.get_resources().empty())
            continue;

        MaterialResourceGroup material_rg{};
        material_rg.resource_group = ResourceGroup::create(layout);
        material_rg.set = set;

        m_resource_groups.push_back(material_rg);
    }

    m_is_baked = true;
    return true;
}

size_t Material::get_hash() const
{
    size_t hash = 0;

    hash ^= std::hash<Shader*>()(get_vertex_shader().get());
    hash ^= std::hash<Shader*>()(get_fragment_shader().get());

    for (const MaterialResourceGroup& resource_group : m_resource_groups)
    {
        // TODO: Don't submit this, implement the ResourceGroup::get_hash() function
        hash ^= std::hash<ResourceGroup*>()(resource_group.resource_group.get());
    }

    return hash;
}

} // namespace Mizu
