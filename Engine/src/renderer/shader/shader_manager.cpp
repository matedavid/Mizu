#include "shader_manager.h"

#include <format>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"
#include "base/utils/hash.h"

#include "renderer/renderer.h"

namespace Mizu
{

ShaderManager& ShaderManager::get()
{
    static ShaderManager instance;
    return instance;
}

void ShaderManager::reset()
{
    m_path_mappings.clear();
    m_shader_cache.clear();
}

void ShaderManager::add_shader_mapping(std::string_view mapping, std::filesystem::path path)
{
    const std::string mapping_str = std::string{mapping};
    if (m_path_mappings.contains(mapping_str))
    {
        MIZU_LOG_WARNING("ShaderManager mapping {} -> {} already exists", mapping, path.string());
        return;
    }

    MIZU_ASSERT(std::filesystem::exists(path), "Trying to add mapping with a path that doesn't exist");
    m_path_mappings.emplace(mapping_str, std::move(path));
}

void ShaderManager::remove_shader_mapping(std::string_view mapping)
{
    const std::string mapping_str = std::string{mapping};

    const auto it = m_path_mappings.find(mapping_str);
    if (it == m_path_mappings.end())
    {
        MIZU_LOG_WARNING("No mapping exists for: {}", mapping);
        return;
    }

    m_path_mappings.erase(it);
}

std::shared_ptr<Shader> ShaderManager::get_shader(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    const size_t hash = get_shader_hash(virtual_path, entry_point, type, environment);

    const auto it = m_shader_cache.find(hash);
    if (it != m_shader_cache.end())
    {
        return it->second;
    }

    const ShaderBytecodeTarget bytecode = get_shader_bytecode_target_for_graphics_api(g_render_device->get_api());

    const auto resolved_path_opt = resolve_path(virtual_path, entry_point, type, environment, bytecode);
    MIZU_ASSERT(
        resolved_path_opt.has_value(),
        "Could not resolve shader path for shader with data: virtual_path={}, entry_point={}, shader_type={}, "
        "bytecode={}",
        virtual_path,
        entry_point,
        static_cast<uint32_t>(type),
        static_cast<uint32_t>(bytecode));

    ShaderDescription desc{};
    desc.path = *resolved_path_opt;
    desc.entry_point = entry_point;
    desc.type = type;

    const auto shader = g_render_device->create_shader(desc);
    m_shader_cache.emplace(hash, shader);

    return shader;
}

const SlangReflection& ShaderManager::get_reflection(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    const size_t hash = get_shader_hash(virtual_path, entry_point, type, environment);

    const auto it = m_reflection_cache.find(hash);
    if (it != m_reflection_cache.end())
    {
        return it->second;
    }

    const ShaderBytecodeTarget bytecode = get_shader_bytecode_target_for_graphics_api(g_render_device->get_api());

    const auto resolved_path_opt = resolve_path(virtual_path, entry_point, type, environment, bytecode);
    MIZU_ASSERT(
        resolved_path_opt.has_value(),
        "Could not resolve shader path for shader with data: virtual_path={}, entry_point={}, shader_type={}, "
        "bytecode={}",
        virtual_path,
        entry_point,
        static_cast<uint32_t>(type),
        static_cast<uint32_t>(bytecode));

    const std::string reflection_path = resolved_path_opt->string() + ".json";

    const std::string json_content = Filesystem::read_file_string(reflection_path);

    const SlangReflection reflection(json_content);
    m_reflection_cache.emplace(hash, reflection);

    return m_reflection_cache.find(hash)->second;
}

std::string ShaderManager::combine_path(
    std::string_view path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment,
    ShaderBytecodeTarget bytecode)
{
    return std::format(
        "{}{}.{}.{}.{}",
        path,
        environment.get_shader_filename_string(),
        entry_point,
        get_shader_type_suffix(type),
        get_shader_bytecode_target_suffix(bytecode));
}

std::optional<std::filesystem::path> ShaderManager::resolve_path(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment,
    ShaderBytecodeTarget bytecode)
{
    const std::string combined_virtual_path = combine_path(virtual_path, entry_point, type, environment, bytecode);
    return resolve_path(combined_virtual_path);
}

std::optional<std::filesystem::path> ShaderManager::resolve_path(std::string_view path)
{
    for (const auto& [mapping, dest] : m_path_mappings)
    {
        const auto resolved_opt = resolve_path(path, mapping, dest.string());
        if (resolved_opt.has_value())
        {
            return resolved_opt;
        }
    }

    return {};
}

std::optional<std::filesystem::path> ShaderManager::resolve_path(
    std::string_view path,
    std::string source,
    std::string dest)
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

size_t ShaderManager::get_shader_hash(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    return hash_compute(virtual_path, entry_point, type, environment.get_hash());
}

std::string_view ShaderManager::get_shader_type_suffix(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return "vertex";
    case ShaderType::Fragment:
        return "fragment";
    case ShaderType::Compute:
        return "compute";
    case ShaderType::RtxRaygen:
        return "raygen";
    case ShaderType::RtxClosestHit:
        return "closesthit";
    case ShaderType::RtxMiss:
        return "miss";
    case ShaderType::RtxIntersection:
        return "intersection";
    case ShaderType::RtxAnyHit:
        return "anyhit";
    }
}

std::string_view ShaderManager::get_shader_bytecode_target_suffix(ShaderBytecodeTarget target)
{
    switch (target)
    {
    case ShaderBytecodeTarget::Dxil:
        return "dxil";
    case ShaderBytecodeTarget::Spirv:
        return "spv";
    }
}

ShaderBytecodeTarget ShaderManager::get_shader_bytecode_target_for_graphics_api(GraphicsApi api)
{
    switch (api)
    {
    case GraphicsApi::Dx12:
        return ShaderBytecodeTarget::Dxil;
    case GraphicsApi::Vulkan:
        return ShaderBytecodeTarget::Spirv;
    }
}

} // namespace Mizu
