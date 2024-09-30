#include "shader_manager.h"

#include "renderer/abstraction/shader.h"
#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu {

std::unordered_map<std::string, std::filesystem::path> ShaderManager::m_mapping_to_path;
std::unordered_map<size_t, std::shared_ptr<GraphicsShader>> ShaderManager::m_id_to_graphics_shader;
std::unordered_map<size_t, std::shared_ptr<ComputeShader>> ShaderManager::m_id_to_compute_shader;

void ShaderManager::clean() {
    m_mapping_to_path.clear();
    m_id_to_graphics_shader.clear();
    m_id_to_compute_shader.clear();
}

void ShaderManager::create_shader_mapping(const std::string& mapping, const std::filesystem::path& path) {
    if (m_mapping_to_path.contains(mapping)) {
        MIZU_LOG_WARNING("ShaderManager mapping {} -> {} already exists", mapping, path.string());
        return;
    }

    m_mapping_to_path.insert({mapping, path});
}

std::shared_ptr<GraphicsShader> ShaderManager::get_shader(const std::string& vertex_path,
                                                          const std::string& fragment_path) {
    const auto vertex_path_resolved = resolve_path(vertex_path);
    const auto fragment_path_resolved = resolve_path(fragment_path);

    MIZU_ASSERT(
        std::filesystem::exists(vertex_path_resolved), "Vertex path does not exist: {}", vertex_path_resolved.string());
    MIZU_ASSERT(std::filesystem::exists(fragment_path_resolved),
                "Fragment path does not exist: {}",
                fragment_path_resolved.string());

    const std::hash<std::string> hasher;
    const size_t hash = hasher(vertex_path_resolved.string()) ^ hasher(fragment_path_resolved.string());

    const auto it = m_id_to_graphics_shader.find(hash);
    if (it != m_id_to_graphics_shader.end()) {
        return it->second;
    }

    const auto shader = GraphicsShader::create(vertex_path_resolved, fragment_path_resolved);
    m_id_to_graphics_shader.insert({hash, shader});

    return shader;
}

std::shared_ptr<ComputeShader> ShaderManager::get_shader(const std::string& compute_path) {
    const auto path_resolved = resolve_path(compute_path);
    MIZU_ASSERT(std::filesystem::exists(path_resolved), "Compute path does not exist: {}", path_resolved.string());

    const std::hash<std::string> hasher;
    const size_t hash = hasher(path_resolved.string());

    const auto it = m_id_to_compute_shader.find(hash);
    if (it != m_id_to_compute_shader.end()) {
        return it->second;
    }

    const auto shader = ComputeShader::create(path_resolved);
    m_id_to_compute_shader.insert({hash, shader});

    return shader;
}

std::filesystem::path ShaderManager::resolve_path(const std::string& path) {
    std::filesystem::path resolved;

    for (const auto& [mapping, real_path] : m_mapping_to_path) {
        const auto pos = path.find(mapping);
        if (pos != std::string::npos) {
            auto rest_of_path = path.substr(pos + mapping.size());
            if (rest_of_path.starts_with("/")) {
                // Could cause problems because it would be treated as an absolute path
                rest_of_path = rest_of_path.substr(1);
            }
            resolved = real_path / rest_of_path;
            break;
        }
    }

    return resolved;
}

} // namespace Mizu
