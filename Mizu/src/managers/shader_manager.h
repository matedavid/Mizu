#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace Mizu {

// Forward declarations
class GraphicsShader;

class ShaderManager {
  public:
    static void create_shader_mapping(const std::string& mapping, const std::filesystem::path& path);

    [[nodiscard]] static std::shared_ptr<GraphicsShader> get_shader(const std::string& vertex_path,
                                                                    const std::string& fragment_path);

  private:
    static std::unordered_map<std::string, std::filesystem::path> m_mapping_to_path;

    static std::filesystem::path resolve_path(const std::string& path);
};

} // namespace Mizu