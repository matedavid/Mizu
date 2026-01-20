#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "render_core/rhi/shader.h"

#include "mizu_render_module.h"
#include "render/shader/shader_compiler.h"

namespace Mizu
{

// Forward declarations
enum class GraphicsApi;

class MIZU_RENDER_API ShaderManager
{
  public:
    static ShaderManager& get();

    void reset();

    void add_shader_mapping(std::string_view mapping, std::filesystem::path path);
    void remove_shader_mapping(std::string_view mapping);

    std::shared_ptr<Shader> get_shader(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment);

    const SlangReflection& get_reflection(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment);

    std::string combine_path(
        std::string_view path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment,
        ShaderBytecodeTarget bytecode);

    std::optional<std::filesystem::path> resolve_path(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment,
        ShaderBytecodeTarget bytecode);
    std::optional<std::filesystem::path> resolve_path(std::string_view path);
    std::optional<std::filesystem::path> resolve_path(std::string_view path, std::string source, std::string dest);

    static size_t get_shader_hash(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment);

    static std::string_view get_shader_type_suffix(ShaderType type);
    static std::string_view get_shader_bytecode_target_suffix(ShaderBytecodeTarget target);
    static ShaderBytecodeTarget get_shader_bytecode_target_for_graphics_api(GraphicsApi api);

  private:
    std::unordered_map<std::string, std::filesystem::path> m_path_mappings;
    std::unordered_map<size_t, std::shared_ptr<Shader>> m_shader_cache;
    std::unordered_map<size_t, SlangReflection> m_reflection_cache;
};

} // namespace Mizu