#pragma once

#include <vector>

#include "render_core/shader/shader_properties.h"

namespace Mizu
{

class ShaderReflection
{
  public:
    ShaderReflection(const std::vector<char>& source);

    [[nodiscard]] std::vector<ShaderInput> get_inputs() const { return m_inputs; }
    [[nodiscard]] std::vector<ShaderProperty> get_properties() const { return m_properties; }
    [[nodiscard]] std::vector<ShaderConstant> get_constants() const { return m_constants; }
    [[nodiscard]] std::vector<ShaderOutput> get_outputs() const { return m_outputs; }

  private:
    std::vector<ShaderInput> m_inputs;
    std::vector<ShaderProperty> m_properties;
    std::vector<ShaderConstant> m_constants;
    std::vector<ShaderOutput> m_outputs;
};

} // namespace Mizu