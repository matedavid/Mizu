#include "shader_group.h"

#include "render_core/rhi/renderer.h"

#include "render_core/shader/shader_properties.h"

#include "utility/assert.h"

namespace Mizu
{

ShaderGroup& ShaderGroup::add_shader(const Shader& shader)
{
    const uint32_t max_descriptor_set = Renderer::get_capabilities().max_resource_group_sets;

    for (const ShaderProperty& property : shader.get_properties())
    {
        MIZU_ASSERT(property.binding_info.set < max_descriptor_set,
                    "Property set is bigger or equal than max descriptor set ({} >= {})",
                    property.binding_info.set,
                    max_descriptor_set);

        if (property.binding_info.set >= m_properties_per_set.size())
        {
            for (size_t i = m_properties_per_set.size(); i < property.binding_info.set + 1; ++i)
                m_properties_per_set.emplace_back();
        }

        auto [it, inserted] = m_resource_to_shader_stages_map.try_emplace(property.name, shader.get_type());

        if (!inserted)
        {
            it->second |= shader.get_type();
        }
        else
        {
            m_properties_per_set[property.binding_info.set].push_back(property);
            m_property_info_map.insert({property.name, property});
        }
    }

    for (const ShaderConstant& constant : shader.get_constants())
    {
        auto [it, inserted] = m_resource_to_shader_stages_map.try_emplace(constant.name, shader.get_type());

        if (!inserted)
        {
            it->second |= shader.get_type();
        }
        else
        {
            m_constants.push_back(constant);
        }
    }

    return *this;
}

const std::vector<ShaderProperty>& ShaderGroup::get_properties_in_set(uint32_t set) const
{
    MIZU_ASSERT(set < m_properties_per_set.size(),
                "Set is bigger than max allowed set ({} >= {})",
                set,
                m_properties_per_set.size());

    return m_properties_per_set[set];
}

const std::vector<ShaderConstant> ShaderGroup::get_constants() const
{
    return m_constants;
}

ShaderProperty ShaderGroup::get_property_info(const std::string& name) const
{
    const auto it = m_property_info_map.find(name);
    MIZU_ASSERT(it != m_property_info_map.end(), "Property {} does not exist", name);

    return it->second;
}

ShaderPropertyBindingInfo ShaderGroup::get_property_binding_info(const std::string& name) const
{
    const auto it = m_property_info_map.find(name);
    MIZU_ASSERT(it != m_property_info_map.end(), "Property {} does not exist", name);

    return it->second.binding_info;
}

ShaderType ShaderGroup::get_resource_stage_bits(const std::string& name) const
{
    const auto it = m_resource_to_shader_stages_map.find(name);
    MIZU_ASSERT(it != m_resource_to_shader_stages_map.end(), "Resource {} does not exist", name);

    return it->second;
}

} // namespace Mizu