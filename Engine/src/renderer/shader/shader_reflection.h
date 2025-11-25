#pragma once
#pragma once
#pragma once

#include <nlohmann/json.hpp>
#include <span>
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

    std::span<const ShaderInputOutput> get_inputs() const { return std::span(m_inputs); }
    std::span<const ShaderInputOutput> get_outputs() const { return std::span(m_outputs); }
    std::span<const ShaderResource> get_parameters() const { return std::span(m_parameters); }
    std::span<const ShaderPushConstant> get_constants() const { return std::span(m_constants); }

  private:
    std::vector<ShaderInputOutput> m_inputs;
    std::vector<ShaderInputOutput> m_outputs;
    std::vector<ShaderResource> m_parameters;
    std::vector<ShaderPushConstant> m_constants;

    void parse_parameters(const nlohmann::json& json_parameters);
    void parse_push_constants(const nlohmann::json& json_push_constants);
    void parse_inputs_outputs(const nlohmann::json& json_inputs_outputs, std::vector<ShaderInputOutput>& out_vector)
        const;

    ShaderPrimitive parse_primitive(const nlohmann::json& json_primitive) const;

    ShaderBindingInfo parse_binding_info(const nlohmann::json& json_binding_info) const;
};

} // namespace Mizu