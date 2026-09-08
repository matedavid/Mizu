#include "render/systems/shader_manager.h"

#include <span>
#include <vector>

#include "asset/asset.h"
#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "core/game_context.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

ShaderManager& ShaderManager::get()
{
    static ShaderManager instance;
    return instance;
}

void ShaderManager::reset()
{
    m_shader_cache.clear();
    m_reflection_cache.clear();
}

static ShaderBytecodeTarget get_shader_bytecode_target_for_graphics_api(GraphicsApi api)
{
    switch (api)
    {
    case GraphicsApi::Dx12:
        return ShaderBytecodeTarget::Dxil;
    case GraphicsApi::Vulkan:
        return ShaderBytecodeTarget::Spirv;
    }
}

static ShaderDeclarationAssetHandle get_shader_declaration_asset_handle(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    const ShaderBytecodeTarget bytecode_target =
        get_shader_bytecode_target_for_graphics_api(g_render_device->get_api());

    return get_shader_declaration_asset_id(
        get_shader_virtual_path(virtual_path, entry_point, type, bytecode_target, environment));
}

std::shared_ptr<Shader> ShaderManager::get_shader(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    const ShaderDeclarationAssetHandle handle =
        get_shader_declaration_asset_handle(virtual_path, entry_point, type, environment);

    const auto it = m_shader_cache.find(handle);
    if (it != m_shader_cache.end())
        return it->second;

    if (!load_shader_and_reflection(handle, entry_point, type))
        return nullptr;

    return m_shader_cache.find(handle)->second;
}

const SlangReflection* ShaderManager::get_reflection(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    const ShaderDeclarationAssetHandle handle =
        get_shader_declaration_asset_handle(virtual_path, entry_point, type, environment);

    const auto it = m_reflection_cache.find(handle);
    if (it != m_reflection_cache.end())
        return &it->second;

    if (!load_shader_and_reflection(handle, entry_point, type))
        return nullptr;

    return &m_reflection_cache.find(handle)->second;
}

size_t ShaderManager::get_shader_hash(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    return get_shader_declaration_asset_handle(virtual_path, entry_point, type, environment).get_id();
}

bool ShaderManager::load_shader_and_reflection(
    ShaderDeclarationAssetHandle handle,
    std::string_view entry_point,
    ShaderType type)
{
    IAssetLoader& asset_loader = g_game_context->get_asset_loader();

    const std::optional<ShaderDeclarationAssetRecord> record = asset_loader.get_shader_declaration_record(handle);
    if (!record.has_value())
    {
        MIZU_ASSERT(false, "Failed to load shader declaration with handle: {}", handle.get_id());
        return false;
    }

    const ShaderDeclarationAssetMetadata& metadata = record->metadata;

    // TODO: Temporal allocation :)
    std::vector<uint8_t> payload(metadata.get_total_size_bytes());
    if (!asset_loader.load_shader_declaration(handle, payload))
    {
        MIZU_ASSERT(false, "Failed to load shader declaration with handle: {}", handle.get_id());
        return false;
    }

    const std::span<uint8_t> bytecode_payload =
        std::span(payload.data() + metadata.bytecode_offset, metadata.bytecode_size);
    const std::span<uint8_t> reflection_payload =
        std::span(payload.data() + metadata.reflection_offset, metadata.reflection_size);

    ShaderDescription desc{};
    desc.bytecode = bytecode_payload;
    desc.entry_point = entry_point;
    desc.type = type;

    const auto shader = g_render_device->create_shader(desc);
    m_shader_cache.emplace(handle, shader);

    const std::string_view reflection_json{
        reinterpret_cast<const char*>(reflection_payload.data()), reflection_payload.size()};

    const SlangReflection reflection(reflection_json);
    m_reflection_cache.emplace(handle, reflection);

    return true;
}

} // namespace Mizu
