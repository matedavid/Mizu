#pragma once

#include <string_view>
#include <vector>

#include "render_core/rhi/shader.h"

#include "renderer/shader/shader_compiler.h"
#include "renderer/shader/shader_manager.h"
#include "renderer/shader/shader_permutation.h"
#include "renderer/shader/shader_registry.h"

namespace Mizu
{

class ShaderDeclaration
{
  public:
    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        [[maybe_unused]] ShaderCompilationEnvironment& environment)
    {
    }

    static bool should_compile_permutation([[maybe_unused]] const ShaderCompilationTarget& target) { return true; }

    std::shared_ptr<Shader> get_shader() const
    {
        const Shader::Description desc = get_shader_description();
        return ShaderManager::get_shader(desc, m_environment);
    }

    using Permutations = PermutationList<>;

  protected:
    ShaderDeclaration() = default;

    template <typename PermutationsT>
    ShaderDeclaration(const PermutationsT& permutations)
    {
        permutations.apply([&](const auto& permutation) { permutation.set_environment(m_environment); });
    }

    virtual Shader::Description get_shader_description() const = 0;

    ShaderCompilationEnvironment m_environment{};
};

#define IMPLEMENT_SHADER_DECLARATION(_shader_path, _shader_type, _shader_entry_point) \
    static std::string_view get_path()                                                 \
    {                                                                                  \
        return _shader_path;                                                           \
    }                                                                                  \
    static ShaderType get_type()                                                       \
    {                                                                                  \
        return _shader_type;                                                           \
    }                                                                                  \
    static std::string_view get_entry_point()                                          \
    {                                                                                  \
        return _shader_entry_point;                                                    \
    }                                                                                  \
    Shader::Description get_shader_description() const override                        \
    {                                                                                  \
        return Shader::Description{                                                    \
            .path = get_path(),                                                        \
            .entry_point = std::string(get_entry_point()),                             \
            .type = get_type(),                                                        \
        };                                                                             \
    }

} // namespace Mizu