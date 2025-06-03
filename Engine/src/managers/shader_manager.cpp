#include "shader_manager.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

std::unordered_map<std::string, std::filesystem::path> ShaderManager::m_mapping_to_path;
std::unordered_map<size_t, std::shared_ptr<GraphicsShader>> ShaderManager::m_id_to_graphics_shader;
std::unordered_map<size_t, std::shared_ptr<ComputeShader>> ShaderManager::m_id_to_compute_shader;

std::unordered_map<size_t, std::shared_ptr<Shader>> ShaderManager::m_id_to_shader;

void ShaderManager::clean()
{
    m_mapping_to_path.clear();
    m_id_to_graphics_shader.clear();
    m_id_to_compute_shader.clear();
    m_id_to_shader.clear();
}

void ShaderManager::create_shader_mapping(const std::string& mapping, const std::filesystem::path& path)
{
    if (m_mapping_to_path.contains(mapping))
    {
        MIZU_LOG_WARNING("ShaderManager mapping {} -> {} already exists", mapping, path.string());
        return;
    }

    m_mapping_to_path.insert({mapping, path});
}

std::shared_ptr<Shader> ShaderManager::get_shader2(const Shader::Description& desc)
{
    const std::filesystem::path resolved_path = resolve_path(desc.path.string());

    MIZU_ASSERT(std::filesystem::exists(resolved_path), "Shader path does not exist: {}", resolved_path.string());

    Shader::Description resolved_desc = desc;
    resolved_desc.path = resolved_path;

    const size_t hash = hash_shader(resolved_desc);

    auto it = m_id_to_shader.find(hash);
    if (it == m_id_to_shader.end())
    {
        const auto shader = Shader::create(resolved_desc);
        it = m_id_to_shader.insert({hash, shader}).first;
    }

    return it->second;
}

std::shared_ptr<GraphicsShader> ShaderManager::get_shader(const ShaderInfo& vert_info, const ShaderInfo& frag_info)
{
    const auto vertex_path_resolved = resolve_path(vert_info.name);
    const auto fragment_path_resolved = resolve_path(frag_info.name);

    MIZU_ASSERT(
        std::filesystem::exists(vertex_path_resolved), "Vertex path does not exist: {}", vertex_path_resolved.string());
    MIZU_ASSERT(std::filesystem::exists(fragment_path_resolved),
                "Fragment path does not exist: {}",
                fragment_path_resolved.string());

    const std::hash<std::string> hasher;
    const size_t hash = hasher(vertex_path_resolved.string()) ^ hasher(fragment_path_resolved.string());

    const auto it = m_id_to_graphics_shader.find(hash);
    if (it != m_id_to_graphics_shader.end())
    {
        return it->second;
    }

    ShaderStageInfo resolved_vert_info{};
    resolved_vert_info.path = vertex_path_resolved;
    resolved_vert_info.entry_point = vert_info.entry_point;

    ShaderStageInfo resolved_frag_info{};
    resolved_frag_info.path = fragment_path_resolved;
    resolved_frag_info.entry_point = frag_info.entry_point;

    const auto shader = GraphicsShader::create(resolved_vert_info, resolved_frag_info);
    m_id_to_graphics_shader.insert({hash, shader});

    return shader;
}

std::shared_ptr<ComputeShader> ShaderManager::get_shader(const ShaderInfo& comp_info)
{
    const auto path_resolved = resolve_path(comp_info.name);
    MIZU_ASSERT(std::filesystem::exists(path_resolved), "Compute path does not exist: {}", path_resolved.string());

    const std::hash<std::string> hasher;
    const size_t hash = hasher(path_resolved.string());

    const auto it = m_id_to_compute_shader.find(hash);
    if (it != m_id_to_compute_shader.end())
    {
        return it->second;
    }

    ShaderStageInfo resolved_comp_info{};
    resolved_comp_info.path = path_resolved;
    resolved_comp_info.entry_point = comp_info.entry_point;

    const auto shader = ComputeShader::create(resolved_comp_info);
    m_id_to_compute_shader.insert({hash, shader});

    return shader;
}

std::filesystem::path ShaderManager::resolve_path(const std::string& path)
{
    std::filesystem::path resolved;

    for (const auto& [mapping, real_path] : m_mapping_to_path)
    {
        const auto pos = path.find(mapping);
        if (pos != std::string::npos)
        {
            auto rest_of_path = path.substr(pos + mapping.size());
            if (rest_of_path.starts_with("/"))
            {
                // Could cause problems because it would be treated as an absolute path
                rest_of_path = rest_of_path.substr(1);
            }
            resolved = real_path / rest_of_path;
            break;
        }
    }

    return resolved;
}

size_t ShaderManager::hash_shader(const Shader::Description& desc)
{
    std::hash<std::string> string_hasher;
    std::hash<ShaderType> type_hasher;

    return string_hasher(desc.path.string()) ^ string_hasher(desc.entry_point) ^ type_hasher(desc.type);
}

} // namespace Mizu
