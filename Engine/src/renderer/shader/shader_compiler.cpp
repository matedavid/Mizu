#include "shader_compiler.h"

#include <array>
#include <format>

#include "base/io/filesystem.h"
#include "renderer/shader/shader_declaration.h"

namespace Mizu
{

#define SLANG_CHECK(expression)                                    \
    do                                                             \
    {                                                              \
        const SlangResult status = expression;                     \
        MIZU_ASSERT(SLANG_SUCCEEDED(status), "Slang call failed"); \
    } while (false)

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

SlangCompiler::SlangCompiler(SlangCompilerDescription desc) : m_description(std::move(desc))
{
    SLANG_CHECK(slang::createGlobalSession(m_global_session.writeRef()));
}

void SlangCompiler::compile(
    const std::string& content,
    const std::filesystem::path& dest_path,
    std::string_view entry_point,
    ShaderBytecodeTarget target) const
{
    Slang::ComPtr<slang::ISession> session;
    create_session(session);

    Slang::ComPtr<slang::IBlob> diagnostics;

    Slang::ComPtr<slang::IModule> shader_module;
    shader_module = session->loadModuleFromSourceString(
        dest_path.string().c_str(), nullptr, content.c_str(), diagnostics.writeRef());
    diagnose(diagnostics);

    MIZU_ASSERT(shader_module != nullptr, "Failed to compile shader");

    Slang::ComPtr<slang::IEntryPoint> module_entry_point;
    SLANG_CHECK(shader_module->findEntryPointByName(entry_point.data(), module_entry_point.writeRef()));

    MIZU_ASSERT(module_entry_point != nullptr, "Failed to find entry point: {}", entry_point);

    const std::array<slang::IComponentType*, 2> component_types = {shader_module, module_entry_point};

    Slang::ComPtr<slang::IComponentType> composed_program;
    SLANG_CHECK(session->createCompositeComponentType(
        component_types.data(), component_types.size(), composed_program.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    Slang::ComPtr<slang::IComponentType> linked_program;
    SLANG_CHECK(composed_program->link(linked_program.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    // Be careful with this, it depends on the order of the ShaderBytecodeTarget and the targets in the slang session
    const uint32_t target_idx = static_cast<uint32_t>(target);

    // We only have one entry point
    const uint32_t entry_point_idx = 0;

    Slang::ComPtr<slang::IBlob> bytecode;
    SLANG_CHECK(
        linked_program->getEntryPointCode(entry_point_idx, target_idx, bytecode.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    const std::string string_bytecode(
        static_cast<const char*>(bytecode->getBufferPointer()), bytecode->getBufferSize());
    Filesystem::write_file_string(dest_path, string_bytecode);
}

void SlangCompiler::create_session(Slang::ComPtr<slang::ISession>& out_session) const
{
    slang::TargetDesc dxil_target{};
    dxil_target.format = SlangCompileTarget::SLANG_DXIL;
    dxil_target.profile = m_global_session->findProfile("sm_6_0");

    slang::TargetDesc spirv_target{};
    spirv_target.format = SlangCompileTarget::SLANG_SPIRV;
    spirv_target.profile = m_global_session->findProfile("spirv_1_5");

    const std::array targets = {dxil_target, spirv_target};

    std::vector<const char*> include_paths(m_description.include_paths.size());
    for (size_t i = 0; i < m_description.include_paths.size(); ++i)
    {
        include_paths[i] = m_description.include_paths[i].data();
    }

    slang::CompilerOptionEntry compiler_option_entry_point_name{};
    compiler_option_entry_point_name.name = slang::CompilerOptionName::VulkanUseEntryPointName;
    compiler_option_entry_point_name.value = slang::CompilerOptionValue{
        .kind = slang::CompilerOptionValueKind::Int,
        .intValue0 = 1,
    };

    std::array compiler_options = {compiler_option_entry_point_name};

    slang::SessionDesc session_desc{};
    session_desc.targets = targets.data();
    session_desc.targetCount = static_cast<uint32_t>(targets.size());
    session_desc.searchPaths = include_paths.data();
    session_desc.searchPathCount = static_cast<uint32_t>(include_paths.size());
    session_desc.compilerOptionEntries = compiler_options.data();
    session_desc.compilerOptionEntryCount = static_cast<uint32_t>(compiler_options.size());
    session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    SLANG_CHECK(m_global_session->createSession(session_desc, out_session.writeRef()));
}

void SlangCompiler::diagnose(const Slang::ComPtr<slang::IBlob>& diagnostics) const
{
    if (diagnostics)
    {
        MIZU_LOG_WARNING("Shader diagnosis: {}", static_cast<const char*>(diagnostics->getBufferPointer()));
    }
}

} // namespace Mizu