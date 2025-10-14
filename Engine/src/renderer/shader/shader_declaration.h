#pragma once

#include <string_view>
#include <vector>

#include "render_core/rhi/shader.h"

#include "renderer/shader/shader_compiler.h"
#include "renderer/shader/shader_permutation.h"
#include "renderer/shader/shader_registry.h"

namespace Mizu
{

struct ShaderDeclarationMetadata
{
    std::string_view path;
    ShaderType type;
    std::string_view entry_point;

    void (*modify_compilation_environment_func)(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment);

    bool (*should_compile_permutation_func)(const ShaderCompilationTarget& target);

    void (*generate_all_permutation_combinations_func)(std::vector<ShaderCompilationEnvironment>&);
};

class ShaderDeclaration2
{
  public:
    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        [[maybe_unused]] ShaderCompilationEnvironment& environment)
    {
    }

    static bool should_compile_permutation([[maybe_unused]] const ShaderCompilationTarget& target) { return true; }

    using Permutations = PermutationList<>;

  protected:
    ShaderDeclaration2() = default;

    template <typename PermutationsT>
    ShaderDeclaration2(const PermutationsT& permutations)
    {
        permutations.apply([&](const auto& permutation) { permutation.set_environment(m_environment); });
    }

    ShaderCompilationEnvironment m_environment{};
};

#define IMPLEMENT_SHADER_DECLARATION(_shader, _shader_path, _shader_type, _shader_entry_point)   \
    ShaderRegistryCallback _##_shader##_callback                                                 \
    {                                                                                            \
        ShaderDeclarationMetadata                                                                \
        {                                                                                        \
            .path = _shader_path, .type = _shader_type, .entry_point = _shader_entry_point,      \
            .modify_compilation_environment_func = ##_shader## ::modify_compilation_environment, \
            .should_compile_permutation_func = ##_shader## ::should_compile_permutation,         \
            .generate_all_permutation_combinations_func =                                        \
                ##_shader## ::Permutations::get_all_permutation_combinations,                    \
        }                                                                                        \
    }

} // namespace Mizu