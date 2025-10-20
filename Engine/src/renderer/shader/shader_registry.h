#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "render_core/rhi/shader.h"

namespace Mizu
{

// Forward declarations
class ShaderCompilationEnvironment;
struct ShaderCompilationTarget;

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

template <typename T>
concept IsShaderDeclaration = requires {
    { T::get_path() } -> std::same_as<std::string_view>;
    { T::get_type() } -> std::same_as<ShaderType>;
    { T::get_entry_point() } -> std::same_as<std::string_view>;
};

class ShaderRegistry
{
  public:
    template <typename T>
    void register_shader()
    {
        static_assert(IsShaderDeclaration<T>, "Template type is not a Shader Declaration");

        register_shader(ShaderDeclarationMetadata{
            .path = T::get_path(),
            .type = T::get_type(),
            .entry_point = T::get_entry_point(),
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

using ShaderProviderFunc = std::function<void(ShaderRegistry&)>;

class ShaderProviderRegistry
{
  public:
    static ShaderProviderRegistry& get();

    void add_shader_provider(ShaderProviderFunc function);
    std::span<const ShaderProviderFunc> get_shader_providers() const;

  private:
    std::vector<ShaderProviderFunc> m_shader_providers;
};

struct ShaderProviderCallback
{
    ShaderProviderCallback(const std::function<void(ShaderRegistry&)> func);
};

#define MIZU_REGISTER_SHADER_PROVIDER(_provider_namespace, _provider_func) \
    ShaderProviderCallback _provider_callback_##_provider_namespace        \
    {                                                                      \
        _provider_func                                                     \
    }

} // namespace Mizu