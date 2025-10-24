#ifndef MIZU_ENGINE_SHADERS_OUTPUT_PATH
#error You must define MIZU_ENGINE_SHADERS_SOURCE_PATH and MIZU_ENGINE_SHADERS_OUTPUT_PATH
#endif

#include <format>
#include <fstream>
#include <iostream>
#include <Mizu/Mizu.h>

#define MIZU_SHADER_PIPELINE_DUMP_SLANG_SOURCE 1
#define MIZU_SHADER_PIPELINE_FORCE_COMPILE 0

using namespace Mizu;

static std::filesystem::path resolve_output_path(
    std::string_view path,
    const std::unordered_map<std::string, std::string>& output_mappings)
{
    for (const auto& [source, dest] : output_mappings)
    {
        const auto path_opt = ShaderManager::resolve_path(path, source, dest);
        if (path_opt.has_value())
        {
            return *path_opt;
        }
    }

    MIZU_UNREACHABLE("No viable path resolve was found for path: {}", path);
}

int main()
{
    ShaderRegistry registry;
    for (const ShaderProviderFunc& func : ShaderProviderRegistry::get().get_shader_providers())
    {
        func(registry);
    }

    std::vector<std::string> include_paths;
    for (const auto& [source, dest] : registry.get_shader_mappings())
    {
        include_paths.push_back(dest);
        ShaderManager::create_shader_mapping(source, dest);
    }

    SlangCompilerDescription slang_compiler_desc{};
    slang_compiler_desc.include_paths = include_paths;

    const SlangCompiler compiler{slang_compiler_desc};

    constexpr std::array BYTECODE_TARGETS = {ShaderBytecodeTarget::Dxil, ShaderBytecodeTarget::Spirv};

    for (const ShaderDeclarationMetadata& metadata : registry.get_shader_metadata_list())
    {
        const auto& source_path_opt = ShaderManager::resolve_path(metadata.path);
        MIZU_ASSERT(source_path_opt.has_value(), "Could not resolve path: {}", metadata.path);

        const std::filesystem::path& source_path = *source_path_opt;
        MIZU_ASSERT(
            std::filesystem::exists(source_path), "Shader source path does not exist: {}", source_path.string());

        const std::string content = Filesystem::read_file_string(source_path);

        std::vector<ShaderCompilationEnvironment> environments;
        environments.reserve(registry.get_shader_metadata_list().size() * 10);

        metadata.generate_all_permutation_combinations_func(environments);

        for (const ShaderCompilationEnvironment& environment : environments)
        {
            ShaderCompilationTarget compilation_target{};
#if MIZU_PLATFORM_WINDOWS
            compilation_target.platform = Platform::Windows;
#elif MIZU_PLATFORM_UNIX
            compilation_target.platform = Platform::Linux;
#else
#error Unspecified platform
#endif

            for (const ShaderBytecodeTarget& target : BYTECODE_TARGETS)
            {
                compilation_target.target = target;

                ShaderCompilationEnvironment target_environment = environment;
                metadata.modify_compilation_environment_func(compilation_target, target_environment);

                const std::filesystem::path& base_dest_path =
                    resolve_output_path(metadata.path, registry.get_shader_output_mappings());
                const std::filesystem::path& dest_path = ShaderManager::resolve_path_suffix(
                    base_dest_path, target_environment, metadata.entry_point, metadata.type, target);

                // Create parent directories if they don't exist
                if (!std::filesystem::exists(dest_path.parent_path()))
                {
                    std::filesystem::create_directories(dest_path.parent_path());
                }

                const uint64_t last_write_time =
                    std::filesystem::last_write_time(source_path).time_since_epoch().count();
                const size_t environment_hash = target_environment.get_hash();

                bool compile_shader = true;

                const std::filesystem::path dest_timestamp_path = dest_path.string() + ".timestamp";
                if (std::filesystem::exists(dest_timestamp_path) && std::filesystem::exists(dest_path))
                {
                    const std::string file_timestamp = Filesystem::read_file_string(dest_timestamp_path);

                    uint64_t file_last_write_time;
                    size_t file_environment_hash;

                    std::istringstream iss(file_timestamp);
                    iss >> file_last_write_time >> file_environment_hash;

                    if (file_last_write_time == last_write_time && file_environment_hash == environment_hash)
                    {
                        compile_shader = false;
                    }
                }

#if !MIZU_SHADER_PIPELINE_FORCE_COMPILE
                if (compile_shader)
#endif
                {
                    const std::string permutation_content = target_environment.get_shader_defines() + content;

#if MIZU_SHADER_PIPELINE_DUMP_SLANG_SOURCE
                    const std::string slang_source_dest_path = std::format("{}.slang", dest_path.string());
                    Filesystem::write_file_string(slang_source_dest_path, permutation_content);
#endif

                    MIZU_LOG_INFO("Compiling: {} -> {}", source_path.string(), dest_path.string());
                    compiler.compile(permutation_content, dest_path, metadata.entry_point, metadata.type, target);

                    const std::string new_timestamp = std::format("{} {}", last_write_time, environment_hash);
                    Filesystem::write_file_string(dest_timestamp_path, new_timestamp);
                }
            }
        }
    }

    return 0;
}