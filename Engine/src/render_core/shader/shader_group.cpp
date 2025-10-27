#include "shader_group.h"

#include "base/debug/assert.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/rhi/renderer.h"
#include "render_core/shader/shader_properties.h"
#include "render_core/shader/shader_reflection.h"

namespace Mizu
{

ShaderGroup& ShaderGroup::add_shader(const Shader& shader)
{
    [[maybe_unused]] const uint32_t max_descriptor_set = Renderer::get_capabilities().max_resource_group_sets;

    const SlangReflection& reflection2 = shader.get_reflection2();

    for (const ShaderResource& parameter : reflection2.get_parameters())
    {
        MIZU_ASSERT(
            parameter.binding_info.set < max_descriptor_set,
            "Property set is bigger or equal than max descriptor set ({} >= {})",
            parameter.binding_info.set,
            max_descriptor_set);

        if (parameter.binding_info.set >= m_parameters_per_set.size())
        {
            for (size_t i = m_parameters_per_set.size(); i < parameter.binding_info.set + 1; ++i)
                m_parameters_per_set.emplace_back();
        }

        auto [it, inserted] = m_resource_to_shader_stages_map.try_emplace(parameter.name, shader.get_type());

        if (!inserted)
        {
            it->second |= shader.get_type();
        }
        else
        {
            m_parameters_per_set[parameter.binding_info.set].push_back(parameter);
            m_parameter_info_map.insert({parameter.name, parameter});
        }
    }

    for (const ShaderPushConstant& constant : reflection2.get_constants())
    {
        auto [it, inserted] = m_resource_to_shader_stages_map.try_emplace(constant.name, shader.get_type());

        if (!inserted)
        {
            it->second |= shader.get_type();
        }
        else
        {
            m_constants.push_back(constant);
            m_constant_info_map.insert({constant.name, constant});
        }
    }

    return *this;
}

const std::vector<ShaderResource>& ShaderGroup::get_parameters_in_set(uint32_t set) const
{
    MIZU_ASSERT(
        set < m_parameters_per_set.size(),
        "Set is bigger than max allowed set ({} >= {})",
        set,
        m_parameters_per_set.size());

    return m_parameters_per_set[set];
}

const std::vector<ShaderPushConstant>& ShaderGroup::get_constants() const
{
    return m_constants;
}

ShaderResource ShaderGroup::get_parameter_info(const std::string& name) const
{
    const auto it = m_parameter_info_map.find(name);
    MIZU_ASSERT(it != m_parameter_info_map.end(), "Property '{}' does not exist", name);

    return it->second;
}

ShaderBindingInfo ShaderGroup::get_parameter_binding_info(const std::string& name) const
{
    const auto it = m_parameter_info_map.find(name);
    MIZU_ASSERT(it != m_parameter_info_map.end(), "Property '{}' does not exist", name);

    return it->second.binding_info;
}

ShaderPushConstant ShaderGroup::get_constant_info(const std::string& name) const
{
    const auto it = m_constant_info_map.find(name);
    MIZU_ASSERT(it != m_constant_info_map.end(), "Constant '{}' does not exist", name);

    return it->second;
}

ShaderType ShaderGroup::get_resource_stage_bits(const std::string& name) const
{
    const auto it = m_resource_to_shader_stages_map.find(name);
    MIZU_ASSERT(it != m_resource_to_shader_stages_map.end(), "Resource '{}' does not exist", name);

    return it->second;
}

} // namespace Mizu