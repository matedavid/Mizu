#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "render_core/rhi/shader.h"

namespace Mizu
{

// Forward declarations
class Shader;
struct ShaderProperty;
struct ShaderConstant;

class ShaderGroup
{
  public:
    ShaderGroup& add_shader(const Shader& shader);

    const std::vector<ShaderProperty>& get_properties_in_set(uint32_t set) const;
    const std::vector<ShaderConstant> get_constants() const;

    ShaderPropertyBindingInfo get_property_binding_info(const std::string& name);

    ShaderType get_resource_stage_bits(const std::string& name) const;

    uint32_t get_max_set() const { return static_cast<uint32_t>(m_properties_per_set.size()); }

  private:
    std::vector<std::vector<ShaderProperty>> m_properties_per_set;
    std::vector<ShaderConstant> m_constants;

    std::unordered_map<std::string, ShaderProperty> m_property_info_map;

    std::unordered_map<std::string, ShaderType> m_resource_to_shader_stages_map;
};

} // namespace Mizu