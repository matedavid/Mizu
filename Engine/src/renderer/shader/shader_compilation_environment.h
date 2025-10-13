#pragma once

#include <string_view>
#include <vector>

namespace Mizu
{

struct ShaderCompilationPermutation
{
    std::string_view define;
    uint32_t value;
};

class ShaderCompilationEnvironment
{
  public:
    void set_define(std::string_view define, uint32_t value)
    {
        m_permutation_values.emplace_back(std::move(define), value);
    }

  private:
    std::vector<ShaderCompilationPermutation> m_permutation_values;
};

} // namespace Mizu
