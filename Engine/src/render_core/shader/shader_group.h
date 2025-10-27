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

    const std::vector<ShaderResource>& get_parameters_in_set(uint32_t set) const;
    const std::vector<ShaderPushConstant>& get_constants() const;

    ShaderResource get_parameter_info(const std::string& name) const;
    ShaderBindingInfo get_parameter_binding_info(const std::string& name) const;

    ShaderPushConstant get_constant_info(const std::string& name) const;

    ShaderType get_resource_stage_bits(const std::string& name) const;

    uint32_t get_max_set() const { return static_cast<uint32_t>(m_parameters_per_set.size()); }

  private:
    std::vector<std::vector<ShaderResource>> m_parameters_per_set;
    std::vector<ShaderPushConstant> m_constants;

    std::unordered_map<std::string, ShaderResource> m_parameter_info_map;
    std::unordered_map<std::string, ShaderPushConstant> m_constant_info_map;

    std::unordered_map<std::string, ShaderType> m_resource_to_shader_stages_map;
};

} // namespace Mizu