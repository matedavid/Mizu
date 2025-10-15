#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "render_core/rhi/shader.h"

#include "renderer/shader/shader_compiler.h"

namespace Mizu
{

// Forward declarations
enum class GraphicsAPI;

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

    static std::filesystem::path resolve_path(std::string_view path);
    static std::filesystem::path resolve_path(
        std::string_view path,
        const std::string& source,
        const std::string& dest);

    static std::filesystem::path resolve_path_suffix(
        std::string_view path,
        const ShaderCompilationEnvironment& environment);
    static std::filesystem::path resolve_path_suffix(
        std::string_view path,
        const ShaderCompilationEnvironment& environment,
        GraphicsAPI api);

    static std::shared_ptr<Shader> get_shader(const Shader::Description& desc);

  private:
    static std::unordered_map<std::string, std::filesystem::path> m_mapping_to_path;
    static std::unordered_map<size_t, std::shared_ptr<Shader>> m_id_to_shader;

    static size_t hash_shader(const Shader::Description& desc);
};

} // namespace Mizu