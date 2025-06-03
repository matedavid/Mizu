#pragma once

#include <vector>

#include "render_core/shader/shader_properties.h"

namespace Mizu
{

class ShaderReflection
{
  public:
    ShaderReflection(const std::vector<char>& source);

    const std::vector<ShaderInput>& get_inputs() const { return m_inputs; }
    const std::vector<ShaderProperty>& get_properties() const { return m_properties; }
    const std::vector<ShaderConstant>& get_constants() const { return m_constants; }
    const std::vector<ShaderOutput>& get_outputs() const { return m_outputs; }

  private:
    std::vector<ShaderInput> m_inputs;
    std::vector<ShaderProperty> m_properties;
    std::vector<ShaderConstant> m_constants;
    std::vector<ShaderOutput> m_outputs;
};

} // namespace Mizu