#pragma once

#include <span>
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

    std::span<const ShaderCompilationPermutation> get_defines() const { return std::span(m_permutation_values); }

  private:
    std::vector<ShaderCompilationPermutation> m_permutation_values;
};

enum class ShaderBytecodeTarget
{
    Dxil,
    Spirv,
};

enum class Platform
{
    Windows,
    Linux,
};

struct ShaderCompilationTarget
{
    ShaderBytecodeTarget target;
    Platform platform;
};

} // namespace Mizu
