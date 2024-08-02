#pragma once

#include <vector>

#include "renderer/abstraction/shader/shader_properties.h"

namespace Mizu {

class ShaderReflection {
  public:
    ShaderReflection(const std::vector<char>& source);

  private:
    std::vector<ShaderInput> m_inputs;
};

} // namespace Mizu