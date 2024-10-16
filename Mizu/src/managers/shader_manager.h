#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace Mizu {

// Forward declarations
class GraphicsShader;
class ComputeShader;
struct ShaderStageInfo;

class ShaderManager {
  public:
    struct ShaderInfo {
        std::string name;
        std::string entry_point;
    };

    static void clean();

    static void create_shader_mapping(const std::string& mapping, const std::filesystem::path& path);

    [[nodiscard]] static std::shared_ptr<GraphicsShader> get_shader(const ShaderInfo& vert_info,
                                                                    const ShaderInfo& frag_info);

    [[nodiscard]] static std::shared_ptr<ComputeShader> get_shader(const ShaderInfo& comp_info);

  private:
    static std::unordered_map<std::string, std::filesystem::path> m_mapping_to_path;

    static std::unordered_map<size_t, std::shared_ptr<GraphicsShader>> m_id_to_graphics_shader;
    static std::unordered_map<size_t, std::shared_ptr<ComputeShader>> m_id_to_compute_shader;

    static std::filesystem::path resolve_path(const std::string& path);
};

} // namespace Mizu