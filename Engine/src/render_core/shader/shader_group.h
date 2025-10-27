#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "render_core/rhi/shader.h"
#include "renderer/shader/shader_types.h"

namespace Mizu
{

// Forward declarations
class Shader;

class ShaderGroup
{
  public:
    ShaderGroup& add_shader(const Shader& shader);

    const std::vector<ShaderProperty>& get_properties_in_set(uint32_t set) const;
    const std::vector<ShaderConstant> get_constants() const;

    const std::vector<ShaderResource>& get_parameters_in_set2(uint32_t set) const;
    const std::vector<ShaderPushConstant> get_constants2() const;

    ShaderProperty get_property_info(const std::string& name) const;
    ShaderPropertyBindingInfo get_property_binding_info(const std::string& name) const;

    ShaderResource get_parameter_info2(const std::string& name) const;
    ShaderBindingInfo get_parameter_binding_info2(const std::string& name) const;

    ShaderConstant get_constant_info(const std::string& name) const;

    ShaderPushConstant get_constant_info2(const std::string& name) const;

    ShaderType get_resource_stage_bits(const std::string& name) const;

    uint32_t get_max_set() const { return static_cast<uint32_t>(m_parameters_per_set2.size()); }

  private:
    std::vector<std::vector<ShaderProperty>> m_properties_per_set;
    std::vector<ShaderConstant> m_constants;

    std::unordered_map<std::string, ShaderProperty> m_property_info_map;
    std::unordered_map<std::string, ShaderConstant> m_constant_info_map;

    std::vector<std::vector<ShaderResource>> m_parameters_per_set2;
    std::vector<ShaderPushConstant> m_constants2;

    std::unordered_map<std::string, ShaderResource> m_parameter_info_map2;
    std::unordered_map<std::string, ShaderPushConstant> m_constant_info_map2;

    std::unordered_map<std::string, ShaderType> m_resource_to_shader_stages_map;
};

} // namespace Mizu