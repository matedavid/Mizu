#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <slang-com-ptr.h>
#include <slang.h>

#include "render_core/rhi/shader.h"

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
        ShaderBytecodeTarget target) const;

  private:
    SlangCompilerDescription m_description{};
    Slang::ComPtr<slang::IGlobalSession> m_global_session;

    void create_session(Slang::ComPtr<slang::ISession>& out_session) const;
    void diagnose(const Slang::ComPtr<slang::IBlob>& diagnostics) const;
};

} // namespace Mizu
