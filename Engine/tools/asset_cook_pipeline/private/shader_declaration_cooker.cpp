#include "shader_declaration_cooker.h"

#include <string_view>

namespace Mizu
{

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

//
// ShaderDeclarationImporter
//

std::span<const std::string_view> ShaderDeclarationImporter::extensions() const
{
    static constexpr std::string_view extensions[]{
        ".slang",
    };

    return extensions;
}

uint32_t ShaderDeclarationImporter::version() const
{
    return 0;
}

bool ShaderDeclarationImporter::should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const
{
    const size_t id = hash_compute(request.virtual_path);

    const uint64_t last_write_time =
        static_cast<uint64_t>(std::filesystem::last_write_time(request.path).time_since_epoch().count());

    const Timestamp ts{
        .ts = last_write_time,
        .version = version(),
    };

    return timestamp_db.is_different(id, ts);
}

void ShaderDeclarationImporter::import(
    const ImportRequest& request,
    const CookContext& context,
    std::vector<CookRequest>& outputs)
{
    const ShaderDeclarationImportPayload* payload = request.payload.get_if<ShaderDeclarationImportPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    // TODO: Probably not best place
    {
        const size_t id = hash_compute(request.virtual_path);

        const uint64_t last_write_time =
            static_cast<uint64_t>(std::filesystem::last_write_time(request.path).time_since_epoch().count());

        const Timestamp ts{
            .ts = last_write_time,
            .version = version(),
        };

        context.timestamp_db.record(id, ts);
    }

    ShaderCompilationTarget compilation_target{};
#if MIZU_PLATFORM_WINDOWS
    compilation_target.platform = Platform::Windows;
#elif MIZU_PLATFORM_UNIX
    compilation_target.platform = Platform::Linux;
#else
#error Unspecified platform
#endif

    meta::enum_array<ShaderBytecodeTarget, bool> enabled_bytecode_targets{};
#if MIZU_RENDER_CORE_DX12_ENABLED
    enabled_bytecode_targets[ShaderBytecodeTarget::Dxil] = true;
#endif
#if MIZU_RENDER_CORE_VULKAN_ENABLED
    enabled_bytecode_targets[ShaderBytecodeTarget::Spirv] = true;
#endif

    const ShaderDeclarationMetadata& metadata = payload->metadata;

    std::vector<ShaderCompilationEnvironment> environments{};
    metadata.generate_all_permutation_combinations_func(environments);

    for (const ShaderBytecodeTarget target : meta::enum_traits<ShaderBytecodeTarget>::values)
    {
        for (ShaderCompilationEnvironment environment : environments)
        {
            if (!enabled_bytecode_targets[target])
                continue;

            compilation_target.target = target;
            metadata.modify_compilation_environment_func(compilation_target, environment);

            environment.set_define(get_shader_define_for_target_bytecode(compilation_target.target), 1);
            environment.set_define(get_shader_define_for_platform(compilation_target.platform), 1);

            outputs.push_back({
                .asset_type = AssetType::ShaderDeclaration,
                .payload =
                    ShaderDeclarationCookPayload{
                        .path = request.path,
                        .bytecode_target = target,
                        .environment = environment,
                    },
            });
        }
    }
}

//
// ShaderDeclarationCooker
//

bool ShaderDeclarationCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
}

void ShaderDeclarationCooker::cook(const CookRequest& request, std::vector<SinkRequest>& outputs)
{
    (void)request;
    (void)outputs;
}

} // namespace Mizu