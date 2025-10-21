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
    const std::filesystem::path& path,
    const ShaderCompilationEnvironment& environment,
    std::string_view entry_point,
    ShaderType type)
{
    return resolve_path_suffix(
        path,
        environment,
        entry_point,
        type,
        get_shader_bytecode_target_for_graphics_api(Renderer::get_config().graphics_api));
}

std::filesystem::path ShaderManager::resolve_path_suffix(
    const std::filesystem::path& path,
    const ShaderCompilationEnvironment& environment,
    std::string_view entry_point,
    ShaderType type,
    ShaderBytecodeTarget target)
{
    const std::string resolved_path = std::format(
        "{}{}.{}.{}.{}",
        path.string(),
        environment.get_shader_filename_string(),
        entry_point,
        get_shader_type_suffix(type),
        get_shader_bytecode_target_suffix(target));
    return std::filesystem::path(resolved_path);
}

std::string ShaderManager::get_shader_type_suffix(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return "vertex";
    case ShaderType::Fragment:
        return "fragment";
    case ShaderType::Compute:
        return "compute";
    }

    MIZU_UNREACHABLE("ShaderType not implemented");
}

std::string ShaderManager::get_shader_bytecode_target_suffix(ShaderBytecodeTarget target)
{
    switch (target)
    {
    case ShaderBytecodeTarget::Dxil:
        return "dxil";
    case ShaderBytecodeTarget::Spirv:
        return "spv";
    }

    MIZU_UNREACHABLE("ShaderBytecodeTarget not implemented");
}

ShaderBytecodeTarget ShaderManager::get_shader_bytecode_target_for_graphics_api(GraphicsAPI api)
{
    switch (api)
    {
    case GraphicsAPI::DirectX12:
        return ShaderBytecodeTarget::Dxil;
    case GraphicsAPI::Vulkan:
        return ShaderBytecodeTarget::Spirv;
    }

    MIZU_UNREACHABLE("GraphicsAPI not implemented");
}

std::shared_ptr<Shader> ShaderManager::get_shader(const Shader::Description& desc)
{
    return ShaderManager::get_shader(desc, ShaderCompilationEnvironment{});
}

std::shared_ptr<Shader> ShaderManager::get_shader(
    const Shader::Description& desc,
    const ShaderCompilationEnvironment& environment)
{
    const auto resolved_path_opt = resolve_path(desc.path.string());
    MIZU_ASSERT(resolved_path_opt.has_value(), "Could not resolve shader path for path: {}", desc.path.string());

    const std::filesystem::path& resolved_path_base = *resolved_path_opt;
    const std::filesystem::path resolved_path =
        resolve_path_suffix(resolved_path_base.string(), environment, desc.entry_point, desc.type);

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
