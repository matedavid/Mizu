#include "shader_compiler.h"

#include <array>
#include <format>

#include "base/io/filesystem.h"
#include "renderer/shader/shader_declaration.h"

namespace Mizu
{

//
// ShaderCompilationEnvironment
//

void ShaderCompilationEnvironment::set_define(std::string_view define, uint32_t value)
{
    m_permutation_values.emplace_back(std::move(define), value, false);
}

std::string ShaderCompilationEnvironment::get_shader_defines() const
{
    // 6 chars for #define, 20 chars for the name, 2 chars for the value
    constexpr size_t EXPECTED_SIZE = 20 + 2;

    std::string content;
    content.reserve(m_permutation_values.size() * EXPECTED_SIZE);

    for (const ShaderCompilationDefine& value : m_permutation_values)
    {
        content += std::format("#define {} {}\n", value.define, value.value);
    }

    return content;
}

std::string ShaderCompilationEnvironment::get_shader_filename_string() const
{
    // 20 chars for the name, 1 char for the underscore, 2 chars for the value
    constexpr size_t EXPECTED_SIZE = 20 + 1 + 2;

    std::string content;
    content.reserve(EXPECTED_SIZE);

    for (const ShaderCompilationDefine& value : m_permutation_values)
    {
        if (!value.is_permutation_value)
            continue;

        content += std::format("_{}_{}", value.define, value.value);
    }

    return content;
}

size_t ShaderCompilationEnvironment::get_hash() const
{
    std::hash<std::string_view> string_hasher;
    std::hash<uint32_t> uint32_hasher;

    size_t hash = 0;
    for (const ShaderCompilationDefine& permutation : m_permutation_values)
    {
        hash ^= string_hasher(permutation.define) ^ uint32_hasher(permutation.value);
    }

    return hash;
}

void ShaderCompilationEnvironment::set_permutation_define(std::string_view define, uint32_t value)
{
    m_permutation_values.emplace_back(std::move(define), value, true);
}

//
// SlangCompiler
//

#ifndef MIZU_SLANG_COMPILER_PATH
#error MIZU_SLANG_COMPILER_PATH is not defined
#endif

SlangCompiler::SlangCompiler(SlangCompilerDescription desc) : m_description(std::move(desc))
{
    /*
    SlangGlobalSessionDesc global_session_desc{};
    slang::createGlobalSession(&global_session_desc, m_global_session.writeRef());

    slang::TargetDesc spirv_target_desc{};
    spirv_target_desc.format = SLANG_SPIRV;
    spirv_target_desc.profile = m_global_session->findProfile("glsl_450");

    m_target_to_target_index.insert({ShaderBytecodeTarget::Spirv, 0});

    slang::TargetDesc dxil_target_desc{};
    dxil_target_desc.format = SLANG_DXIL;
    dxil_target_desc.profile = m_global_session->findProfile("sm_6_0");

    m_target_to_target_index.insert({ShaderBytecodeTarget::Dxil, 0});


    const std::array targets = {spirv_target_desc, dxil_target_desc};

    std::vector<const char*> search_paths;
    search_paths.reserve(m_description.include_paths.size());

    for (const std::string& include_path : m_description.include_paths)
    {
        search_paths.push_back(include_path.c_str());
    }

    slang::SessionDesc session_desc{};
    session_desc.targets = targets.data();
    session_desc.targetCount = targets.size();
    session_desc.searchPaths = search_paths.data();
    session_desc.searchPathCount = search_paths.size();

    m_global_session->createSession(session_desc, m_session.writeRef());
    */
}

void SlangCompiler::compile(
    const std::string& content,
    const std::filesystem::path& dest_path,
    std::string_view entry_point,
    ShaderType type,
    ShaderBytecodeTarget target) const
{
    // TODO: Temporal implementation using the actual cli tool, because was having problems using the slang api
    (void)type;

    std::string include_paths;
    for (const std::string& include_path : m_description.include_paths)
    {
        include_paths += " -I" + include_path;
    }

    Filesystem::write_file_string(dest_path, content);

    std::string compile_command;
    if (target == ShaderBytecodeTarget::Dxil)
    {
        compile_command = std::format(
            "{} {} -lang slang {} -o {} -profile sm_6_0 -target dxil -entry {}",
            MIZU_SLANG_COMPILER_PATH,
            include_paths,
            dest_path.string(),
            dest_path.string(),
            entry_point);
    }
    else if (target == ShaderBytecodeTarget::Spirv)
    {
        compile_command = std::format(
            "{} {} -lang slang -fvk-use-entrypoint-name {} -o {} -profile glsl_450 -target spirv -entry {}",
            MIZU_SLANG_COMPILER_PATH,
            include_paths,
            dest_path.string(),
            dest_path.string(),
            entry_point);
    }
    else
    {
        MIZU_UNREACHABLE("ShaderBytecodeTarget not implemented");
    }

    const int32_t result = std::system(compile_command.c_str());
    MIZU_ASSERT(result == 0, "Failed to compile shader: {}", dest_path.string());

    /*
    Slang::ComPtr<slang::IBlob> diagnostics;

    const std::string module_name = dest_path.filename().string();
    slang::IModule* module = m_session->loadModuleFromSourceString(
        module_name.c_str(), dest_path.string().c_str(), content.c_str(), diagnostics.writeRef());

    if (diagnostics)
    {
        MIZU_LOG_WARNING("Warning while loading module: {}", (const char*)diagnostics->getBufferPointer());
    }

    Slang::ComPtr<slang::IEntryPoint> module_entry_point;
    module->findEntryPointByName(entry_point.data(), module_entry_point.writeRef());

    slang::IComponentType* components[] = {module, module_entry_point};
    Slang::ComPtr<slang::IComponentType> program;
    m_session->createCompositeComponentType(components, 2, program.writeRef());

    slang::ProgramLayout* layout = program->getLayout();
    slang::EntryPointReflection* entry_point_reflection = layout->findEntryPointByName(entry_point.data());
    MIZU_ASSERT(entry_point_reflection != nullptr, "Could not find entry point reflection");

    MIZU_ASSERT(
        slang_stage_to_mizu_shader_type(entry_point_reflection->getStage()) == type,
        "Shader stage does not match shader type");

    Slang::ComPtr<slang::IComponentType> linked_program;
    program->link(linked_program.writeRef(), diagnostics.writeRef());

    if (diagnostics)
    {
        MIZU_LOG_WARNING("Warning while linking shader: {}", diagnostics->getBufferPointer());
    }

    int32_t target_index = -1;

    const auto it = m_target_to_target_index.find(target);
    if (it != m_target_to_target_index.end())
    {
        target_index = it->second;
    }

    MIZU_ASSERT(target_index != -1, "Invalid target index");

    const int32_t entry_point_index = 0;

    Slang::ComPtr<slang::IBlob> kernel_blob;
    linked_program->getEntryPointCode(entry_point_index, target_index, kernel_blob.writeRef(), diagnostics.writeRef());

    if (diagnostics)
    {
        MIZU_LOG_WARNING("Warning while compiling shader: {}", (const char*)diagnostics->getBufferPointer());
    }

    const std::string output_string((const char*)kernel_blob->getBufferPointer(), kernel_blob->getBufferSize());
    Filesystem::write_file_string(dest_path, output_string);
    */
}

/*
ShaderType SlangCompiler::slang_stage_to_mizu_shader_type(SlangStage stage)
{
    switch (stage)
    {
    case SLANG_STAGE_VERTEX:
        return ShaderType::Vertex;
    case SLANG_STAGE_FRAGMENT:
        return ShaderType::Fragment;
    case SLANG_STAGE_COMPUTE:
        return ShaderType::Compute;
    default:
        MIZU_UNREACHABLE("Not implemented");
        return ShaderType::Vertex;
    }
}
*/

} // namespace Mizu