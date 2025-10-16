#pragma once

#include <string_view>
#include <tuple>
#include <unordered_map>

#include "base/debug/assert.h"

#include "renderer/shader/shader_compiler.h"

namespace Mizu
{

struct ShaderPermutation
{
    void modify_compilation_environment(uint32_t permutation_id, ShaderCompilationEnvironment& environment) const
    {
        environment.set_permutation_define(get_name(), permutation_id);
    }

    virtual std::string_view get_name() const = 0;
    virtual uint32_t get_count() const = 0;
};

struct ShaderPermutationBool : public ShaderPermutation
{
    using value_type = bool;

    ShaderPermutationBool() : ShaderPermutationBool(false) {}
    ShaderPermutationBool(bool value) : m_value(value) {}

    uint32_t get_value() const { return m_value ? 1 : 0; }
    bool get_native_value() const { return m_value; }

    void set_value(bool value) { m_value = value; }

    void set_environment(ShaderCompilationEnvironment& environment) const
    {
        modify_compilation_environment(get_value(), environment);
    }

    uint32_t get_count() const override { return 2; }

  private:
    bool m_value = false;
};

template <typename EnumT>
struct ShaderPermutationEnum : public ShaderPermutation
{
    static_assert(std::is_enum_v<EnumT>, "Template parameter must be an enum");
    static_assert(requires { EnumT::Count; }, "Enum must have a 'Count' member as a last member");

    using value_type = EnumT;

    ShaderPermutationEnum() : ShaderPermutationEnum(static_cast<EnumT>(0)) {}
    ShaderPermutationEnum(EnumT value) : m_value(value) {}

    uint32_t get_value() const { return static_cast<uint32_t>(m_value); }
    EnumT get_native_value() const { return m_value; }

    void set_value(EnumT value) { m_value = value; }

    void set_environment(ShaderCompilationEnvironment& environment) const
    {
        modify_compilation_environment(get_value(), environment);
    }

    uint32_t get_count() const override { return static_cast<uint32_t>(EnumT::Count); }

  private:
    EnumT m_value;
};

#define MIZU_SHADER_PERMUTATION_BOOL(_name)                 \
  public                                                    \
    ShaderPermutationBool                                   \
    {                                                       \
      public:                                               \
        using ShaderPermutationBool::ShaderPermutationBool; \
                                                            \
        std::string_view get_name() const override          \
        {                                                   \
            return _name;                                   \
        }                                                   \
    }

#define MIZU_SHADER_PERMUTATION_ENUM(_name, _enum)                 \
  public                                                           \
    ShaderPermutationEnum<_enum>                                   \
    {                                                              \
      public:                                                      \
        using ShaderPermutationEnum<_enum>::ShaderPermutationEnum; \
                                                                   \
        std::string_view get_name() const override                 \
        {                                                          \
            return _name;                                          \
        }                                                          \
    }

template <typename... Args>
class PermutationList
{
    static_assert(
        (std::is_default_constructible_v<Args> && ...),
        "All permutation types must be default-constructible");
    static_assert(
        (std::is_base_of_v<ShaderPermutation, Args> && ...),
        "All permutation types must inherit from ShaderPermutation");

  public:
    PermutationList() = default;

    PermutationList(typename Args::value_type... args)
        requires(sizeof...(Args) > 0)
        : m_permutations(Args{args}...)
    {
    }

    template <typename Func>
    void apply(const Func& func) const
    {
        apply_impl(func, std::index_sequence_for<Args...>{});
    }

    static void get_all_permutation_combinations(std::vector<ShaderCompilationEnvironment>& environments)
    {
        ShaderCompilationEnvironment base_environment;
        generate_permutation_combinations_impl(base_environment, environments, std::index_sequence_for<Args...>{});
    }

    template <typename T>
    static constexpr bool has_permutation = (std::is_same_v<T, Args> || ...);

    template <typename T>
    T::value_type get_permutation_value() const
    {
        static_assert(has_permutation<T>, "PermutationList does not contain permutation");

        const T& permutation = std::get<T>(m_permutations);
        return permutation.get_native_value();
    }

    template <typename T>
    void set_permutation_value(T::value_type value)
    {
        static_assert(has_permutation<T>, "PermutationList does not contain permutation");

        T& permutation = std::get<T>(m_permutations);
        permutation.set_value(value);
    }

  private:
    std::tuple<Args...> m_permutations{};
    std::unordered_map<std::string, uint32_t> m_permutation_idx_map;

    template <typename Func, size_t... I>
    void apply_impl(const Func& func, std::index_sequence<I...>) const
    {
        (func(std::get<I>(m_permutations)), ...);
    }

    template <size_t... I>
    static void generate_permutation_combinations_impl(
        const ShaderCompilationEnvironment& base_env,
        std::vector<ShaderCompilationEnvironment>& environments,
        std::index_sequence<I...>)
    {
        generate_permutation_recursive<0, I...>(base_env, environments);
    }

    template <size_t Index, size_t... I>
    static void generate_permutation_recursive(
        ShaderCompilationEnvironment current_environment,
        std::vector<ShaderCompilationEnvironment>& environments)
    {
        if constexpr (Index == sizeof...(I))
        {
            environments.push_back(current_environment);
        }
        else
        {
            const auto& permutation = std::get<Index>(std::tuple<Args...>{});
            const uint32_t count = permutation.get_count();

            for (uint32_t i = 0; i < count; ++i)
            {
                ShaderCompilationEnvironment environment_copy = current_environment;
                permutation.modify_compilation_environment(i, environment_copy);

                generate_permutation_recursive<Index + 1, I...>(environment_copy, environments);
            }
        }
    }
};

} // namespace Mizu