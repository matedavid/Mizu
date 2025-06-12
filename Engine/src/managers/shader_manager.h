#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "render_core/rhi/shader.h"

namespace Mizu
{

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

    static std::shared_ptr<Shader> get_shader(const Shader::Description& desc);

  private:
    static std::unordered_map<std::string, std::filesystem::path> m_mapping_to_path;
    static std::unordered_map<size_t, std::shared_ptr<Shader>> m_id_to_shader;

    static std::filesystem::path resolve_path(const std::string& path);
    static size_t hash_shader(const Shader::Description& desc);
};

} // namespace Mizu