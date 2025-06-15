#include "shader_manager.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

std::unordered_map<std::string, std::filesystem::path> ShaderManager::m_mapping_to_path;
std::unordered_map<size_t, std::shared_ptr<Shader>> ShaderManager::m_id_to_shader;

void ShaderManager::clean()
{
    m_mapping_to_path.clear();
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

std::shared_ptr<Shader> ShaderManager::get_shader(const Shader::Description& desc)
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
