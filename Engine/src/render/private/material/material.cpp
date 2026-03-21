#include "render/material/material.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/utils/hash.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/shader.h"

#include "render/runtime/renderer.h"
#include "render/systems/shader_manager.h"

namespace Mizu
{

Material::Material(MaterialShaderInstance shader_instance)
    : m_shader_instance(shader_instance)
    , m_shader_reflection(
          ShaderManager::get().get_reflection(
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

void Material::set_texture_srv(const std::string& name, std::shared_ptr<ImageResource> resource)
{
    const ShaderBindingInfo binding_info =
        get_resource_binding_info(m_shader_reflection, name, ShaderResourceType::TextureSrv);

    MaterialData data{};
    data.resource = resource;
    data.type = ShaderResourceType::TextureSrv;
    data.binding = binding_info.binding;
    data.set = binding_info.set;

    m_resources.push_back(data);
}

void Material::set_sampler_state(const std::string& name, std::shared_ptr<SamplerState> resource)
{
    const ShaderBindingInfo binding_info =
        get_resource_binding_info(m_shader_reflection, name, ShaderResourceType::SamplerState);

    MaterialData data{};
    data.resource = resource;
    data.type = ShaderResourceType::SamplerState;
    data.binding = binding_info.binding;
    data.set = binding_info.set;

    m_resources.push_back(data);
}

bool Material::bake()
{
    m_resource_groups.clear();

    if (m_resources.empty())
    {
        m_is_baked = true;
        m_pipeline_hash = get_pipeline_hash_internal();
        m_material_hash = get_material_hash_internal();
        return true;
    }

    // Find max binding set
    uint32_t max_binding_set = 0;

    for (const MaterialData& data : m_resources)
    {
        max_binding_set = std::max(max_binding_set, data.set);
    }

    // Group resources by set
    std::vector<std::vector<const MaterialData*>> set_to_resources(max_binding_set + 1);

    for (const MaterialData& data : m_resources)
    {
        set_to_resources[data.set].push_back(&data);
    }

    // Bake descriptor sets
    for (uint32_t set = 0; set < set_to_resources.size(); ++set)
    {
        const std::vector<const MaterialData*>& resources = set_to_resources[set];
        if (resources.empty())
            continue;

        std::unordered_map<size_t, const MaterialData*> unique_map;
        unique_map.reserve(resources.size());

        for (const MaterialData* data : resources)
        {
            const size_t key = hash_compute(data->binding, static_cast<uint32_t>(data->type));
            unique_map.insert_or_assign(key, data);
        }

        std::vector<const MaterialData*> unique_resources{};
        unique_resources.reserve(unique_map.size());
        for (const auto& [_, data] : unique_map)
        {
            unique_resources.push_back(data);
        }

        std::sort(unique_resources.begin(), unique_resources.end(), [](const MaterialData* a, const MaterialData* b) {
            if (a->binding != b->binding)
                return a->binding < b->binding;
            return static_cast<uint32_t>(a->type) < static_cast<uint32_t>(b->type);
        });

        std::vector<DescriptorItem> layout_items{};
        layout_items.reserve(unique_resources.size());

        std::vector<WriteDescriptor> write_descriptors{};
        write_descriptors.reserve(unique_resources.size());

        for (const MaterialData* data : unique_resources)
        {
            switch (data->type)
            {
            case ShaderResourceType::TextureSrv: {
                layout_items.push_back(DescriptorItem::TextureSrv(data->binding, 1, ShaderType::Fragment));

                const auto& resource = std::get<std::shared_ptr<ImageResource>>(data->resource);
                write_descriptors.push_back(
                    WriteDescriptor::TextureSrv(data->binding, ImageResourceView::create(resource)));
                break;
            }
            case ShaderResourceType::SamplerState: {
                layout_items.push_back(DescriptorItem::SamplerState(data->binding, 1, ShaderType::Fragment));

                const auto& resource = std::get<std::shared_ptr<SamplerState>>(data->resource);
                write_descriptors.push_back(WriteDescriptor::SamplerState(data->binding, resource));
                break;
            }
            default:
                MIZU_UNREACHABLE("Unsupported material shader resource type {}", static_cast<uint32_t>(data->type));
                break;
            }
        }

        const DescriptorSetLayoutHandle layout = g_render_device->create_descriptor_set_layout(
            DescriptorSetLayoutDescription{
                .layout = layout_items,
                .vulkan_apply_binding_offsets = false,
            });

        const auto descriptor_set =
            g_render_device->allocate_descriptor_set(layout, DescriptorSetAllocationType::Persistent);
        descriptor_set->update(write_descriptors);

        MaterialDescriptorSet material_set{};
        material_set.descriptor_set = descriptor_set;
        material_set.set = set;

        m_resource_groups.push_back(material_set);
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
    std::vector<size_t> per_resource_hashes{};
    per_resource_hashes.reserve(m_resources.size());

    for (const MaterialData& data : m_resources)
    {
        size_t resource_ptr_hash = 0;

        if (std::holds_alternative<std::shared_ptr<ImageResource>>(data.resource))
        {
            resource_ptr_hash = hash_compute(std::get<std::shared_ptr<ImageResource>>(data.resource).get());
        }
        else if (std::holds_alternative<std::shared_ptr<SamplerState>>(data.resource))
        {
            resource_ptr_hash = hash_compute(std::get<std::shared_ptr<SamplerState>>(data.resource).get());
        }

        per_resource_hashes.push_back(
            hash_compute(data.set, data.binding, static_cast<uint32_t>(data.type), resource_ptr_hash));
    }

    std::sort(per_resource_hashes.begin(), per_resource_hashes.end());

    size_t hash = 0;
    for (const size_t value : per_resource_hashes)
    {
        hash_combine(hash, value);
    }

    return hash;
}

} // namespace Mizu
