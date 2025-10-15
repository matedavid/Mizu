#if !defined(MIZU_ENGINE_SHADERS_SOURCE_PATH) || !defined(MIZU_ENGINE_SHADERS_OUTPUT_PATH)
#error You must define MIZU_ENGINE_SHADERS_SOURCE_PATH and MIZU_ENGINE_SHADERS_OUTPUT_PATH
#endif

#include <fstream>
#include <iostream>
#include <Mizu/Mizu.h>

// #include <slang.h>

using namespace Mizu;

int main()
{
    const ShaderRegistry& registry = ShaderRegistry::get();
    const std::array<GraphicsAPI, 2> graphics_apis = {GraphicsAPI::DirectX12, GraphicsAPI::Vulkan};

    for (const ShaderDeclarationMetadata& metadata : registry.get_shader_metadata_list())
    {
        const std::filesystem::path resolved_path =
            ShaderManager::resolve_path(metadata.path, "EngineShaders", MIZU_ENGINE_SHADERS_SOURCE_PATH);
        const std::filesystem::path dest_path =
            ShaderManager::resolve_path(metadata.path, "EngineShaders", MIZU_ENGINE_SHADERS_OUTPUT_PATH);

        MIZU_LOG_INFO("Shader: {}, {}", resolved_path.string(), metadata.entry_point);

        std::vector<ShaderCompilationEnvironment> environments;
        environments.reserve(10);

        metadata.generate_all_permutation_combinations_func(environments);

        for (const GraphicsAPI& api : graphics_apis)
        {
            for (const ShaderCompilationEnvironment& environment : environments)
            {
                const std::filesystem::path permutation_path =
                    ShaderManager::resolve_path_suffix(dest_path.string(), environment, api);

                MIZU_LOG_INFO("  Permutation: {}", permutation_path.string());
                for (const ShaderCompilationPermutation& permutation : environment.get_defines())
                {
                    MIZU_LOG_INFO("    #define {} {}", permutation.define, permutation.value);
                }
            }
        }

        MIZU_LOG_INFO("");
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