#include "shader_manager.h"

#include <format>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "render_core/rhi/renderer.h"

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

    MIZU_ASSERT(std::filesystem::exists(path), "Trying to add mapping with a path that doesn't exist");
    m_mapping_to_path.insert({mapping, path});
}

void ShaderManager::remove_shader_mapping(const std::string& mapping)
{
    const auto it = m_mapping_to_path.find(mapping);
    if (it == m_mapping_to_path.end())
    {
        MIZU_LOG_WARNING("No mapping exists for: {}", mapping);
        return;
    }

    m_mapping_to_path.erase(it);
}

std::optional<std::filesystem::path> ShaderManager::resolve_path(std::string_view path)
{
    for (const auto& [mapping, real_path] : m_mapping_to_path)
    {
        const std::optional<std::filesystem::path>& resolved = resolve_path(path, mapping, real_path.string());
        if (resolved.has_value())
        {
            return resolved;
        }
    }

    return {};
}

std::optional<std::filesystem::path> ShaderManager::resolve_path(
    std::string_view path,
    std::string_view source,
    std::string_view dest)
{
    const size_t pos = path.find(source);
    if (pos == std::string::npos)
    {
        return {};
    }

    std::filesystem::path resolved;
    std::string_view rest_of_path = path.substr(pos + source.size());
    if (rest_of_path.starts_with("/"))
    {
        // Could cause problems because it would be treated as an absolute path
        rest_of_path = rest_of_path.substr(1);
    }

    return std::filesystem::path(dest) / rest_of_path;
}

std::filesystem::path ShaderManager::resolve_path_suffix(
    std::string_view path,
    const ShaderCompilationEnvironment& environment)
{
    return resolve_path_suffix(path, environment, Renderer::get_config().graphics_api);
}

std::filesystem::path ShaderManager::resolve_path_suffix(
    std::string_view path,
    const ShaderCompilationEnvironment& environment,
    GraphicsAPI api)
{
    std::string suffix;
    for (const ShaderCompilationDefine& parameter : environment.get_defines())
    {
        suffix += std::format("_{}_{}", parameter.define, parameter.value);
    }

    switch (api)
    {
    case GraphicsAPI::DirectX12:
        suffix += ".dxil";
        break;
    case GraphicsAPI::Vulkan:
        suffix += ".spv";
        break;
    }

    return std::filesystem::path(std::string(path) + suffix);
}

std::shared_ptr<Shader> ShaderManager::get_shader(const Shader::Description& desc)
{
    const auto resolved_path_opt = resolve_path(desc.path.string());
    MIZU_ASSERT(resolved_path_opt.has_value(), "Could not resolve shader path for path: {}", desc.path.string());

    const std::filesystem::path& resolved_path = *resolved_path_opt;

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

size_t ShaderManager::hash_shader(const Shader::Description& desc)
{
    std::hash<std::string> string_hasher;
    std::hash<ShaderType> type_hasher;

    return string_hasher(desc.path.string()) ^ string_hasher(desc.entry_point) ^ type_hasher(desc.type);
}

} // namespace Mizu
