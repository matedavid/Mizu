#include "shader_declaration_cook_request_source.h"

#include <string_view>

#include "base/debug/logging.h"

namespace Mizu
{

void ShaderDeclarationCookRequestSource::init(const CookContext& context)
{
    (void)context;

    const ShaderProviderRegistry& provider_registry = ShaderProviderRegistry::get();

    for (IShaderProvider* const provider : provider_registry.get_shader_providers())
    {
        provider->register_shaders(m_registry);
    }

    m_shader_metadata = m_registry.get_shader_metadata_list();
    m_shader_cursor = 0;

#if MIZU_RENDER_CORE_DX12_ENABLED
    m_enabled_bytecode_targets[ShaderBytecodeTarget::Dxil] = true;
#endif
#if MIZU_RENDER_CORE_VULKAN_ENABLED
    m_enabled_bytecode_targets[ShaderBytecodeTarget::Spirv] = true;
#endif
}

uint32_t ShaderDeclarationCookRequestSource::enumerate_n(
    const CookContext&,
    uint32_t number,
    std::vector<CookRequest>& outputs)
{
    uint32_t num_enumerated = 0;

    while (num_enumerated < number)
    {
        if (m_pending_cook_infos.empty())
        {
            if (m_shader_cursor >= m_shader_metadata.size())
                break;

            const ShaderDeclarationMetadata& metadata = m_shader_metadata[m_shader_cursor];
            create_cook_requests_for_metadata(metadata, m_pending_cook_infos);

            m_shader_cursor += 1;

            continue;
        }

        ShaderDeclarationCookInfo cook_info = m_pending_cook_infos.back();
        m_pending_cook_infos.pop_back();

        const CookRequest request{
            .asset_type = AssetCookType::ShaderDeclaration,
            .cook_info = cook_info,
        };

        outputs.push_back(std::move(request));

        num_enumerated += 1;
    }

    return num_enumerated;
}

static std::string_view get_shader_define_for_target_bytecode(ShaderBytecodeTarget target)
{
    switch (target)
    {
    case ShaderBytecodeTarget::Dxil:
        return "MIZU_TARGET_DXIL";
    case ShaderBytecodeTarget::Spirv:
        return "MIZU_TARGET_SPIRV";
    }
}

static std::string_view get_shader_define_for_platform(Platform platform)
{
    switch (platform)
    {
    case Platform::Windows:
        return "MIZU_PLATFORM_WINDOWS";
    case Platform::Linux:
        return "MIZU_PLATFORM_LINUX";
    }
}

void ShaderDeclarationCookRequestSource::create_cook_requests_for_metadata(
    const ShaderDeclarationMetadata& metadata,
    std::vector<ShaderDeclarationCookInfo>& outputs)
{
    ShaderCompilationTarget compilation_target{};
#if MIZU_PLATFORM_WINDOWS
    compilation_target.platform = Platform::Windows;
#elif MIZU_PLATFORM_UNIX
    compilation_target.platform = Platform::Linux;
#else
#error Unspecified platform
#endif

    const ShaderMappingTable& mapping_table = m_registry.get_shader_mapping_table();

    std::vector<ShaderCompilationEnvironment> environments{};
    metadata.generate_all_permutation_combinations_func(environments);

    for (const ShaderBytecodeTarget target : meta::enum_traits<ShaderBytecodeTarget>::values)
    {
        for (ShaderCompilationEnvironment environment : environments)
        {
            if (!m_enabled_bytecode_targets[target])
                continue;

            compilation_target.target = target;
            metadata.modify_compilation_environment_func(compilation_target, environment);

            environment.set_define(get_shader_define_for_target_bytecode(compilation_target.target), 1);
            environment.set_define(get_shader_define_for_platform(compilation_target.platform), 1);

            const std::optional<std::filesystem::path> path = mapping_table.resolve(metadata.virtual_path);
            if (!path.has_value())
            {
                MIZU_LOG_ERROR("Could not resolve physical path for shader: '{}'", metadata.virtual_path);
                continue;
            }

            outputs.push_back(
                ShaderDeclarationCookInfo{
                    .path = *path,
                    .bytecode_target = target,
                    .environment = environment,
                });
        }
    }
}

} // namespace Mizu