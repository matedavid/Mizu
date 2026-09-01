#include "shader_declaration_cooker.h"

#include <string>

#include "asset/asset_metadata.h"
#include "base/debug/assert.h"
#include "base/io/filesystem.h"
#include "base/utils/hash.h"
#include "shader/shader_compiler.h"

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
                .virtual_path = std::string{metadata.virtual_path},
                .asset_mount = request.asset_mount,
                .payload =
                    ShaderDeclarationCookPayload{
                        .path = request.path,
                        .entry_point = metadata.entry_point,
                        .shader_type = metadata.type,
                        .bytecode_target = target,
                        .environment = environment,
                        .include_paths = payload->include_paths,
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

void ShaderDeclarationCooker::cook(
    const CookRequest& request,
    const CookContext& context,
    std::vector<SinkRequest>& outputs)
{
    const ShaderDeclarationCookPayload* payload = request.payload.get_if<ShaderDeclarationCookPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    const std::string content = Filesystem::read_file_string(payload->path);
    const std::string full_content = payload->environment.get_shader_defines() + content;

    const ShaderCompilerDescription shader_compiler_desc{
        .target = payload->bytecode_target,
        .include_paths = payload->include_paths,
    };

    ShaderCompiler compiler{shader_compiler_desc};

    const ShaderCompilerResult result = compiler.compile(full_content, payload->entry_point, payload->shader_type);
    if (!result.success)
        return;

    ShaderDeclarationAssetMetadata metadata{};
    metadata.bytecode_size = result.bytecode.size();
    metadata.reflection_size = result.reflection.size();
    metadata.bytecode_offset = 0;
    metadata.reflection_offset = metadata.bytecode_size;

    const size_t total_size =
        TOTAL_SHADER_DECLARATION_METADATA_SIZE + metadata.bytecode_size + metadata.reflection_size;

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for ShaderDeclaration");

    shader_declaration_serialize_metadata(metadata, data);

    const size_t data_offset = TOTAL_SHADER_DECLARATION_METADATA_SIZE;

    const size_t bytecode_offset = data_offset + metadata.bytecode_offset;
    const size_t reflection_offset = data_offset + metadata.reflection_offset;

    memcpy(data.data() + bytecode_offset, result.bytecode.data(), metadata.bytecode_size);
    memcpy(data.data() + reflection_offset, result.reflection.data(), metadata.reflection_size);

    const std::string shader_virtual_path = get_shader_virtual_path(
        request.virtual_path,
        payload->entry_point,
        payload->shader_type,
        payload->bytecode_target,
        payload->environment);

    const std::string filename = std::to_string(get_shader_declaration_asset_id(shader_virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });
}

} // namespace Mizu