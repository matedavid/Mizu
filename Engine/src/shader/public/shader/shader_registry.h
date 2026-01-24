#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "render_core/rhi/shader.h"

#include "mizu_shader_module.h"

namespace Mizu
{

// Forward declarations
class ShaderCompilationEnvironment;
struct ShaderCompilationTarget;

struct ShaderDeclarationMetadata
{
    std::string_view virtual_path;
    std::string_view entry_point;
    ShaderType type;

    void (*modify_compilation_environment_func)(
        const ShaderCompilationTarget&,
        ShaderCompilationEnvironment& environment);

    bool (*should_compile_permutation_func)(const ShaderCompilationTarget& target);

    void (*generate_all_permutation_combinations_func)(std::vector<ShaderCompilationEnvironment>&);
};

template <typename T>
concept IsShaderDeclaration = requires {
    { T::get_virtual_path() } -> std::same_as<std::string_view>;
    { T::get_entry_point() } -> std::same_as<std::string_view>;
    { T::get_type() } -> std::same_as<ShaderType>;
};

class MIZU_SHADER_API ShaderRegistry
{
  public:
    template <typename T>
    void register_shader()
    {
        static_assert(IsShaderDeclaration<T>, "Template type is not a Shader Declaration");

        register_shader(ShaderDeclarationMetadata{
            .virtual_path = T::get_virtual_path(),
            .entry_point = T::get_entry_point(),
            .type = T::get_type(),
            .modify_compilation_environment_func = T::modify_compilation_environment,
            .should_compile_permutation_func = T::should_compile_permutation,
            .generate_all_permutation_combinations_func = T::Permutations::get_all_permutation_combinations,
        });
    }

    void register_shader(const ShaderDeclarationMetadata& metadata);
    std::span<const ShaderDeclarationMetadata> get_shader_metadata_list() const;

    void add_shader_mapping(std::string source, std::string dest);
    const std::unordered_map<std::string, std::string>& get_shader_mappings() const;

  private:
    std::vector<ShaderDeclarationMetadata> m_shader_metadata_list;

    std::unordered_map<std::string, std::string> m_shader_mapping_map;
};

class MIZU_SHADER_API IShaderProvider
{
  public:
    virtual ~IShaderProvider() = default;
    virtual void register_shaders(ShaderRegistry& registry) = 0;
};

class MIZU_SHADER_API ShaderProviderRegistry
{
  public:
    static ShaderProviderRegistry& get();

    ~ShaderProviderRegistry();

    template <typename T>
    void add_shader_provider()
    {
        m_shader_providers.push_back(new T{});
    }

    std::span<IShaderProvider* const> get_shader_providers() const
    {
        return std::span(m_shader_providers.data(), m_shader_providers.size());
    }

  private:
    std::vector<IShaderProvider*> m_shader_providers;
};

template <typename T>
struct ShaderProviderCallback
{
    ShaderProviderCallback() { ShaderProviderRegistry::get().add_shader_provider<T>(); }
};

#define MIZU_REGISTER_SHADER_PROVIDER(_provider_type) \
    ShaderProviderCallback<_provider_type> g_##_provider_type##_callback {}

} // namespace Mizu