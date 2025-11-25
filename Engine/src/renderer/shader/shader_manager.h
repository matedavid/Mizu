#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "render_core/rhi/shader.h"

#include "renderer/shader/shader_compiler.h"

namespace Mizu
{

// Forward declarations
enum class GraphicsApi;

class ShaderManager
{
  public:
    struct ShaderInfo
    {
        std::string name;
        std::string entry_point;
    };

    static void clean();

    static void create_shader_mapping(const std::string& mapping, const std::filesystem::path& path);
    static void remove_shader_mapping(const std::string& mapping);

    static std::optional<std::filesystem::path> resolve_path(std::string_view path);
    static std::optional<std::filesystem::path> resolve_path(
        std::string_view path,
        std::string_view source,
        std::string_view dest);

    static std::filesystem::path resolve_path_suffix(
        const std::filesystem::path& path,
        const ShaderCompilationEnvironment& environment,
        std::string_view entry_point,
        ShaderType type);
    static std::filesystem::path resolve_path_suffix(
        const std::filesystem::path& path,
        const ShaderCompilationEnvironment& environment,
        std::string_view entry_point,
        ShaderType type,
        ShaderBytecodeTarget target);

    static std::string get_shader_type_suffix(ShaderType type);
    static std::string get_shader_bytecode_target_suffix(ShaderBytecodeTarget target);
    static ShaderBytecodeTarget get_shader_bytecode_target_for_graphics_api(GraphicsApi api);

    static std::shared_ptr<Shader> get_shader(const Shader::Description& desc);
    static std::shared_ptr<Shader> get_shader(
        const Shader::Description& desc,
        const ShaderCompilationEnvironment& environment);

  private:
    static std::unordered_map<std::string, std::filesystem::path> m_mapping_to_path;
    static std::unordered_map<size_t, std::shared_ptr<Shader>> m_id_to_shader;

    static size_t hash_shader(const Shader::Description& desc);
};

} // namespace Mizu