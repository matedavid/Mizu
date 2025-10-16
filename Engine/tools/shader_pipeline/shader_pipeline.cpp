#if !defined(MIZU_ENGINE_SHADERS_SOURCE_PATH) || !defined(MIZU_ENGINE_SHADERS_OUTPUT_PATH)
#error You must define MIZU_ENGINE_SHADERS_SOURCE_PATH and MIZU_ENGINE_SHADERS_OUTPUT_PATH
#endif

#include <format>
#include <fstream>
#include <iostream>
#include <Mizu/Mizu.h>

#define MIZU_SHADER_PIPELINE_DUMP_SLANG_SOURCE 1

using namespace Mizu;

static std::string shader_type_to_suffix(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return "vertex";
    case ShaderType::Fragment:
        return "fragment";
    case ShaderType::Compute:
        return "compute";
    default:
        MIZU_UNREACHABLE("Not implemented");
    }
}

static std::string bytecode_target_to_suffix(ShaderBytecodeTarget target)
{
    switch (target)
    {
    case ShaderBytecodeTarget::Dxil:
        return "dxil";
    case ShaderBytecodeTarget::Spirv:
        return "spv";
    }
}

int main()
{
    SlangCompilerDescription slang_compiler_desc{};
    SlangCompiler compiler{slang_compiler_desc};

    const ShaderRegistry& registry = ShaderRegistry::get();
    std::array bytecode_targets = {ShaderBytecodeTarget::Dxil, ShaderBytecodeTarget::Spirv};

    for (const ShaderDeclarationMetadata& metadata : registry.get_shader_metadata_list())
    {
        const std::filesystem::path source_path =
            *ShaderManager::resolve_path(metadata.path, "EngineShaders", MIZU_ENGINE_SHADERS_SOURCE_PATH);
        const std::filesystem::path dest_path =
            *ShaderManager::resolve_path(metadata.path, "EngineShaders", MIZU_ENGINE_SHADERS_OUTPUT_PATH);

        MIZU_ASSERT(std::filesystem::exists(source_path), "Shader file does not exist: {}", source_path.string());

        std::vector<ShaderCompilationEnvironment> environments;
        environments.reserve(10);

        metadata.generate_all_permutation_combinations_func(environments);

        const std::string& base_shader_content = Filesystem::read_file_string(source_path);

        for (const ShaderCompilationEnvironment& environment : environments)
        {
            ShaderCompilationTarget compilation_target{};
#if MIZU_PLATFORM_WINDOWS
            compilation_target.platform = Platform::Windows;
#elif MIZU_PLATFORM_UNIX
            compilation_target.platform = Platform::Linux;
#else
#error Unespecified platform
#endif

            for (const ShaderBytecodeTarget& target : bytecode_targets)
            {
                compilation_target.target = target;

                ShaderCompilationEnvironment target_environment = environment;
                metadata.modify_compilation_environment_func(compilation_target, target_environment);

                const std::string permutation_dest_path = std::format(
                    "{}{}.{}.{}",
                    dest_path.string(),
                    target_environment.get_shader_filename_string(),
                    shader_type_to_suffix(metadata.type),
                    bytecode_target_to_suffix(target));

                MIZU_LOG_INFO("{} ({}) -> {}", source_path.string(), metadata.entry_point, permutation_dest_path);

                const std::string permutation_content = target_environment.get_shader_defines() + base_shader_content;
                compiler.compile(
                    permutation_content, permutation_dest_path, metadata.entry_point, metadata.type, target);

#if MIZU_SHADER_PIPELINE_DUMP_SLANG_SOURCE
                const std::string slang_source_dest_path = std::format("{}.slang", permutation_dest_path);
                Filesystem::write_file_string(slang_source_dest_path, permutation_content);
#endif
            }
        }
    }

    /*
    SlangSession* session = spCreateSession(nullptr);
        SlangCompileRequest* request = spCreateCompileRequest(session);
    int translationUnitIndex = spAddTranslationUnit(request, SLANG_SOURCE_LANGUAGE_SLANG, "MyShader");

    spAddTranslationUnitSourceFile(request, translationUnitIndex, "myshader.slang");
    int entryPointIndex = spAddEntryPoint(request, translationUnitIndex, "main", SLANG_STAGE_COMPUTE);
    int targetIndex = spAddCodeGenTarget(request, SLANG_SPIRV); // or SLANG_DXIL, SLANG_GLSL, etc.
    (void)targetIndex;

    SlangResult result = spCompile(request);

    if (result != SLANG_OK)
    {
        const char* diagnostics = spGetDiagnosticOutput(request);
        std::cerr << "Compilation failed:\n" << diagnostics << std::endl;
        return 1;
    }

    // Retrieve compiled binary (e.g., SPIR-V, DXIL, etc.)
    size_t codeSize = 0;
    const void* code = spGetEntryPointCode(request, entryPointIndex, &codeSize);
    (void)code;

    // Save to disk or use it directly
    // std::fstream file = std::ifstream::open("output.spv", "wb");
    // fwrite(code, 1, codeSize, file);
    // fclose(file);

    spDestroyCompileRequest(request);
    spDestroySession(session);
    */
}