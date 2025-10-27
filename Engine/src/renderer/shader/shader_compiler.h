#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <slang-com-ptr.h>
#include <slang.h>

#include "render_core/rhi/shader.h"
#include "renderer/shader/shader_reflection.h"

namespace Mizu
{

struct ShaderCompilationDefine
{
    std::string_view define;
    uint32_t value;
    bool is_permutation_value;
};

class ShaderCompilationEnvironment
{
  public:
    void set_define(std::string_view define, uint32_t value);

    std::span<const ShaderCompilationDefine> get_defines() const { return std::span(m_permutation_values); }

    std::string get_shader_defines() const;
    std::string get_shader_filename_string() const;

    size_t get_hash() const;

  private:
    std::vector<ShaderCompilationDefine> m_permutation_values;

    void set_permutation_define(std::string_view define, uint32_t value);

    friend struct ShaderPermutation;
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

struct SlangCompilerDescription
{
    std::vector<std::string> include_paths;
};

class SlangCompiler
{
  public:
    SlangCompiler(SlangCompilerDescription desc);

    void compile(
        const std::string& content,
        const std::filesystem::path& dest_path,
        std::string_view entry_point,
        ShaderType type,
        ShaderBytecodeTarget target) const;

  private:
    SlangCompilerDescription m_description{};
    Slang::ComPtr<slang::IGlobalSession> m_global_session;

    void create_session(Slang::ComPtr<slang::ISession>& out_session) const;

    std::string get_reflection_info(
        const Slang::ComPtr<slang::IComponentType>& program,
        uint32_t target_idx,
        uint32_t entry_point_idx,
        const std::unordered_set<std::string>& push_constant_resources) const;
    void get_push_constant_reflection_info(
        const Slang::ComPtr<slang::IComponentType>& program,
        std::unordered_set<std::string>& push_constant_resources) const;
    ShaderPrimitive get_primitive_reflection(slang::VariableLayoutReflection* layout) const;
    ShaderPrimitiveType get_primitive_type_reflection(slang::TypeLayoutReflection* layout) const;

    void diagnose(const Slang::ComPtr<slang::IBlob>& diagnostics) const;
    static SlangStage mizu_shader_type_to_slang_stage(ShaderType type);
};

} // namespace Mizu
