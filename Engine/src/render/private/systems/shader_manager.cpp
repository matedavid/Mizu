#include "render/systems/shader_manager.h"

#include <span>
#include <vector>

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "core/game_context.h"
#include "shader/shader_asset.h"

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

std::shared_ptr<Shader> ShaderManager::get_shader(ShaderAssetHandle handle)
{
    const auto it = m_shader_cache.find(handle);
    if (it != m_shader_cache.end())
        return it->second;

    const ShaderBytecodeTarget bytecode_target =
        get_shader_bytecode_target_for_graphics_api(g_render_device->get_api());

    if (!load_shader_and_reflection(handle, bytecode_target))
        return nullptr;

    return m_shader_cache.find(handle)->second;
}

const SlangReflection* ShaderManager::get_reflection(ShaderAssetHandle handle)
{
    const auto it = m_reflection_cache.find(handle);
    if (it != m_reflection_cache.end())
        return &it->second;

    const ShaderBytecodeTarget bytecode_target =
        get_shader_bytecode_target_for_graphics_api(g_render_device->get_api());

    if (!load_shader_and_reflection(handle, bytecode_target))
        return nullptr;

    return &m_reflection_cache.find(handle)->second;
}

bool ShaderManager::load_shader_and_reflection(ShaderAssetHandle handle, ShaderBytecodeTarget target)
{
    IAssetLoader& asset_loader = g_game_context->get_asset_loader();

    const std::optional<ShaderAssetRecord> record = asset_loader.get_shader_record(handle, target);
    if (!record.has_value())
    {
        MIZU_ASSERT(false, "Failed to load shader with handle: {}", handle.get_id());
        return false;
    }

    const ShaderAssetMetadata& metadata = record->metadata;

    // TODO: Temporal allocation :)
    std::vector<uint8_t> payload(metadata.get_total_size_bytes());
    if (!asset_loader.load_shader_payload(handle, target, payload))
    {
        MIZU_ASSERT(false, "Failed to load shader with handle: {}", handle.get_id());
        return false;
    }

    const std::span<uint8_t> bytecode_payload =
        std::span(payload.data() + metadata.bytecode_offset, metadata.bytecode_size);
    const std::span<uint8_t> reflection_payload =
        std::span(payload.data() + metadata.reflection_offset, metadata.reflection_size);

    ShaderDescription desc{};
    desc.bytecode = bytecode_payload;
    desc.entry_point = metadata.get_entry_point();
    desc.type = metadata.shader_type;

    const auto shader = g_render_device->create_shader(desc);
    m_shader_cache.emplace(handle, shader);

    const std::string_view reflection_json{
        reinterpret_cast<const char*>(reflection_payload.data()), reflection_payload.size()};

    const SlangReflection reflection(reflection_json);
    m_reflection_cache.emplace(handle, reflection);

    return true;
}

std::shared_ptr<Shader> get_shader(const ShaderDeclaration& declaration)
{
    return get_shader(declaration.get_instance());
}

std::shared_ptr<Shader> get_shader(const ShaderInstance& instance)
{
    return get_shader(instance.virtual_path, instance.entry_point, instance.type, instance.environment);
}

std::shared_ptr<Shader> get_shader(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    return get_shader(get_shader_declaration_asset_handle(virtual_path, entry_point, type, environment));
}

std::shared_ptr<Shader> get_shader(ShaderAssetHandle handle)
{
    return ShaderManager::get().get_shader(handle);
}

const SlangReflection* get_shader_reflection(const ShaderDeclaration& declaration)
{
    return get_shader_reflection(declaration.get_instance());
}

const SlangReflection* get_shader_reflection(const ShaderInstance& instance)
{
    return get_shader_reflection(instance.virtual_path, instance.entry_point, instance.type, instance.environment);
}

const SlangReflection* get_shader_reflection(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    return get_shader_reflection(get_shader_declaration_asset_handle(virtual_path, entry_point, type, environment));
}

const SlangReflection* get_shader_reflection(ShaderAssetHandle handle)
{
    return ShaderManager::get().get_reflection(handle);
}

} // namespace Mizu
