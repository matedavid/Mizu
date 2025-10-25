#pragma once
#pragma once
#pragma once

#include <nlohmann/json.hpp>
#include <string_view>
#include <vector>

#include "render_core/rhi/shader.h"
#include "renderer/shader/shader_types.h"

namespace Mizu
{

class SlangReflection
{
  public:
    SlangReflection(std::string_view data);

  private:
    std::string m_entry_point;
    ShaderType m_shader_type;

    std::vector<ShaderInputOutput> m_inputs;
    std::vector<ShaderInputOutput> m_outputs;
    std::vector<ShaderResource> m_parameters;
    std::vector<ShaderPushConstant> m_constants;

    void parse_entry_point(const nlohmann::json& data);
    ShaderInputOutput parse_entry_point_input_output(const nlohmann::json& data) const;

    void parse_resources(const nlohmann::json& data);
    ShaderResourceConstantBuffer parse_resource_constant_buffer(const nlohmann::json& data) const;

    ShaderPrimitive parse_primitive(const nlohmann::json& data) const;
    ShaderPrimitiveType parse_primitive_type(const nlohmann::json& data) const;
    ShaderPrimitiveType parse_primitive_type_scalar(const nlohmann::json& data) const;
    ShaderPrimitiveType parse_primitive_type_composed(const nlohmann::json& data) const;
};

} // namespace Mizu