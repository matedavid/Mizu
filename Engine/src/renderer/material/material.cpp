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
    std::vector<ResourceGroupBuilder> set_to_resource_group_builder(max_binding_set + 1);

    for (const MaterialData& data : m_resources)
    {
        const ShaderProperty& property = data.property;
        const ShaderType stage_bits = m_shader_group.get_resource_stage_bits(property.name);

        ResourceGroupBuilder& builder = set_to_resource_group_builder[property.binding_info.set];

        ResourceGroupItem item{};

        if (std::holds_alternative<std::shared_ptr<ImageResourceView>>(data.value))
        {
            const ShaderImageProperty& image_property = std::get<ShaderImageProperty>(property.value);
            const auto& value = std::get<std::shared_ptr<ImageResourceView>>(data.value);

            switch (image_property.type)
            {
            case ShaderImageProperty::Type::Sampled:
            case ShaderImageProperty::Type::Separate:
                item = ResourceGroupItem::SampledImage(property.binding_info.binding, value, stage_bits);
                break;
            case ShaderImageProperty::Type::Storage:
                item = ResourceGroupItem::StorageImage(property.binding_info.binding, value, stage_bits);
                break;
            }
        }
        else if (std::holds_alternative<std::shared_ptr<BufferResource>>(data.value))
        {
            const ShaderBufferProperty& buffer_property = std::get<ShaderBufferProperty>(property.value);
            const auto& value = std::get<std::shared_ptr<BufferResource>>(data.value);

            switch (buffer_property.type)
            {
            case ShaderBufferProperty::Type::Uniform:
                item = ResourceGroupItem::UniformBuffer(property.binding_info.binding, value, stage_bits);
                break;
            case ShaderBufferProperty::Type::Storage:
                item = ResourceGroupItem::StorageBuffer(property.binding_info.binding, value, stage_bits);
                break;
            }
        }
        else if (std::holds_alternative<std::shared_ptr<SamplerState>>(data.value))
        {
            const auto& value = std::get<std::shared_ptr<SamplerState>>(data.value);
            item = ResourceGroupItem::Sampler(property.binding_info.binding, value, stage_bits);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid material data or not implemented");
        }

        builder.add_resource(item);
    }

    // Bake resource groups
    for (uint32_t set = 0; set < set_to_resource_group_builder.size(); ++set)
    {
        const ResourceGroupBuilder& layout = set_to_resource_group_builder[set];
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
        hash ^= resource_group.resource_group->get_hash();
    }

    return hash;
}

} // namespace Mizu
