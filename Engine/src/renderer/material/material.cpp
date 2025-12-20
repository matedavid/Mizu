#include "material.h"

#include <string_view>

#include "renderer/renderer.h"
#include "renderer/shader/shader_manager.h"

#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/shader.h"

namespace Mizu
{

Material::Material(MaterialShaderInstance shader_instance)
    : m_shader_instance(shader_instance)
    , m_shader_reflection(ShaderManager::get().get_reflection(
          m_shader_instance.virtual_path,
          m_shader_instance.entry_point,
          ShaderType::Fragment,
          ShaderCompilationEnvironment{}))
{
}

static ShaderBindingInfo get_resource_binding_info(
    const SlangReflection& reflection,
    std::string_view name,
    [[maybe_unused]] ShaderResourceType type)
{
    for (const ShaderResource& resource : reflection.get_parameters())
    {
        if (resource.name == name)
        {
            MIZU_ASSERT(
                resource.type == type,
                "Resource types do not match ({} = {})",
                static_cast<uint32_t>(resource.type),
                static_cast<uint32_t>(type));

            return resource.binding_info;
        }
    }

    MIZU_UNREACHABLE("Shader resource binding info not found: {}", name);
    return ShaderBindingInfo{};
}

void Material::set_texture_srv(const std::string& name, std::shared_ptr<ShaderResourceView> resource)
{
    const ShaderBindingInfo binding_info =
        get_resource_binding_info(m_shader_reflection, name, ShaderResourceType::TextureSrv);

    MaterialData data{};
    data.item = ResourceGroupItem::TextureSrv(binding_info.binding, resource, ShaderType::Fragment);
    data.set = binding_info.set;

    m_resources.push_back(data);
}

void Material::set_buffer_srv(const std::string& name, std::shared_ptr<ShaderResourceView> resource)
{
    const ShaderBindingInfo binding_info =
        get_resource_binding_info(m_shader_reflection, name, ShaderResourceType::StructuredBufferSrv);

    MaterialData data{};
    data.item = ResourceGroupItem::BufferSrv(binding_info.binding, resource, ShaderType::Fragment);
    data.set = binding_info.set;

    m_resources.push_back(data);
}

void Material::set_buffer_cbv(const std::string& name, std::shared_ptr<ConstantBufferView> resource)
{
    const ShaderBindingInfo binding_info =
        get_resource_binding_info(m_shader_reflection, name, ShaderResourceType::ConstantBuffer);

    MaterialData data{};
    data.item = ResourceGroupItem::ConstantBuffer(binding_info.binding, resource, ShaderType::Fragment);
    data.set = binding_info.set;

    m_resources.push_back(data);
}

void Material::set_sampler_state(const std::string& name, std::shared_ptr<SamplerState> resource)
{
    const ShaderBindingInfo binding_info =
        get_resource_binding_info(m_shader_reflection, name, ShaderResourceType::SamplerState);

    MaterialData data{};
    data.item = ResourceGroupItem::Sampler(binding_info.binding, resource, ShaderType::Fragment);
    data.set = binding_info.set;

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
        material_rg.resource_group = g_render_device->create_resource_group(layout);
        material_rg.set = set;

        m_resource_groups.push_back(material_rg);
    }

    m_is_baked = true;

    m_pipeline_hash = get_pipeline_hash_internal();
    m_material_hash = get_material_hash_internal();

    return true;
}

size_t Material::get_pipeline_hash_internal() const
{
    return ShaderManager::get_shader_hash(
        m_shader_instance.virtual_path,
        m_shader_instance.entry_point,
        ShaderType::Fragment,
        ShaderCompilationEnvironment{});
}

size_t Material::get_material_hash_internal() const
{
    size_t hash = 0;

    for (const MaterialResourceGroup& resource_group : m_resource_groups)
    {
        hash ^= resource_group.resource_group->get_hash();
    }

    return hash;
}

} // namespace Mizu
