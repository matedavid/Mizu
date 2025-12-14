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

struct ShaderInstance
{
    std::string_view virtual_path;
    std::string_view entry_point;
    ShaderType type;
    // TODO: I don't like this being a value, contains vector so copy is expensive
    ShaderCompilationEnvironment environment;

    // ShaderInstance(
    //     std::string_view virtual_path_,
    //     std::string_view entry_point_,
    //     ShaderType type_,
    //     const ShaderCompilationEnvironment& environment_)
    //     : virtual_path(virtual_path_)
    //     , entry_point(entry_point_)
    //     , type(type_)
    //     , environment(environment_)
    // {
    // }
};

struct ShaderDeclarationDescription
{
    std::string_view virtual_path;
    std::string_view entry_point;
    ShaderType type;
};

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
        const ShaderDeclarationDescription desc = get_shader_description();
        return ShaderManager::get().get_shader(desc.virtual_path, desc.entry_point, desc.type, m_environment);
    }

    const SlangReflection& get_reflection() const
    {
        const ShaderDeclarationDescription desc = get_shader_description();
        return ShaderManager::get().get_reflection(desc.virtual_path, desc.entry_point, desc.type, m_environment);
    }

    size_t get_hash() const
    {
        const ShaderDeclarationDescription desc = get_shader_description();
        return ShaderManager::get_shader_hash(desc.virtual_path, desc.entry_point, desc.type, m_environment);
    }

    ShaderInstance get_instance() const
    {
        const ShaderDeclarationDescription desc = get_shader_description();
        return ShaderInstance{desc.virtual_path, desc.entry_point, desc.type, m_environment};
    }

    using Permutations = PermutationList<>;

  protected:
    ShaderDeclaration() = default;

    template <typename PermutationsT>
    ShaderDeclaration(const PermutationsT& permutations)
    {
        permutations.apply([&](const auto& permutation) { permutation.set_environment(m_environment); });
    }

    virtual ShaderDeclarationDescription get_shader_description() const = 0;

    ShaderCompilationEnvironment m_environment{};
};

#define IMPLEMENT_SHADER_DECLARATION(_virtual_shader_path, _shader_type, _shader_entry_point) \
    static std::string_view get_virtual_path()                                                \
    {                                                                                         \
        return _virtual_shader_path;                                                          \
    }                                                                                         \
    static Mizu::ShaderType get_type()                                                        \
    {                                                                                         \
        return _shader_type;                                                                  \
    }                                                                                         \
    static std::string_view get_entry_point()                                                 \
    {                                                                                         \
        return _shader_entry_point;                                                           \
    }                                                                                         \
    Mizu::ShaderDeclarationDescription get_shader_description() const override                \
    {                                                                                         \
        return Mizu::ShaderDeclarationDescription{                                            \
            .virtual_path = get_virtual_path(),                                               \
            .entry_point = get_entry_point(),                                                 \
            .type = get_type(),                                                               \
        };                                                                                    \
    }

} // namespace Mizu