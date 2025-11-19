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

void Material::set_texture_srv(const std::string& name, std::shared_ptr<ShaderResourceView> resource)
{
    const ShaderResource& resource_info = m_shader_group.get_parameter_info(name);
    MIZU_ASSERT(
        std::holds_alternative<ShaderResourceTexture>(resource_info.value)
            && std::get<ShaderResourceTexture>(resource_info.value).access == ShaderResourceAccessType::ReadOnly,
        "Resource {} is not a texture SRV",
        name);

    MaterialData data{};
    data.item = ResourceGroupItem::TextureSrv(
        resource_info.binding_info.binding, resource, m_shader_group.get_resource_stage_bits(name));
    data.set = resource_info.binding_info.set;

    m_resources.push_back(data);
}

void Material::set_buffer_srv(const std::string& name, std::shared_ptr<ShaderResourceView> resource)
{
    const ShaderResource& resource_info = m_shader_group.get_parameter_info(name);

    MaterialData data{};
    data.item = ResourceGroupItem::BufferSrv(
        resource_info.binding_info.binding, resource, m_shader_group.get_resource_stage_bits(name));
    data.set = resource_info.binding_info.set;

    m_resources.push_back(data);
}

void Material::set_buffer_cbv(const std::string& name, std::shared_ptr<ConstantBufferView> resource)
{
    const ShaderResource& resource_info = m_shader_group.get_parameter_info(name);

    MaterialData data{};
    data.item = ResourceGroupItem::ConstantBuffer(
        resource_info.binding_info.binding, resource, m_shader_group.get_resource_stage_bits(name));
    data.set = resource_info.binding_info.set;

    m_resources.push_back(data);
}

void Material::set_sampler_state(const std::string& name, std::shared_ptr<SamplerState> resource)
{
    const ShaderResource& resource_info = m_shader_group.get_parameter_info(name);

    MaterialData data{};
    data.item = ResourceGroupItem::Sampler(
        resource_info.binding_info.binding, resource, m_shader_group.get_resource_stage_bits(name));
    data.set = resource_info.binding_info.set;

    m_resources.push_back(data);
}

bool Material::bake()
{
    // Find max binding set
    uint32_t max_binding_set = 0;

    for (const MaterialData& data : m_resources)
    {
        max_binding_set = std::max(max_binding_set, data.set);
    }

    // Create resource groups
    std::vector<ResourceGroupBuilder> set_to_resource_group_builder(max_binding_set + 1);

    for (const MaterialData& data : m_resources)
    {
        ResourceGroupBuilder& builder = set_to_resource_group_builder[data.set];
        builder.add_resource(data.item);
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

    m_pipeline_hash = compute_pipeline_hash();
    m_material_hash = compute_material_hash();

    return true;
}

size_t Material::compute_pipeline_hash() const
{
    std::hash<Shader*> hasher;

    return hasher(m_vertex_shader.get()) ^ hasher(m_fragment_shader.get());
}

size_t Material::compute_material_hash() const
{
    size_t hash = 0;

    for (const MaterialResourceGroup& resource_group : m_resource_groups)
    {
        hash ^= resource_group.resource_group->get_hash();
    }

    return hash;
}

} // namespace Mizu
