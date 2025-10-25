#include "shader_compiler.h"

#include <array>
#include <format>
#include <nlohmann/json.hpp>

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
    ShaderType type,
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

    slang::ProgramLayout* layout = linked_program->getLayout(target_idx, diagnostics.writeRef());
    diagnose(diagnostics);
    MIZU_ASSERT(layout != nullptr, "Linked program layout is nullptr");

    const SlangStage slang_stage = layout->getEntryPointByIndex(entry_point_idx)->getStage();
    MIZU_ASSERT(
        slang_stage == mizu_shader_type_to_slang_stage(type), "Requested shader type does not match with shader stage");

    Slang::ComPtr<slang::IBlob> bytecode;
    SLANG_CHECK(
        linked_program->getEntryPointCode(entry_point_idx, target_idx, bytecode.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    Filesystem::write_file(
        dest_path, static_cast<const char*>(bytecode->getBufferPointer()), bytecode->getBufferSize());

    // Dump reflection
    // TODO: At the moment only
    Slang::ComPtr<slang::IBlob> json_reflection;
    SLANG_CHECK(layout->toJson(json_reflection.writeRef()));

    // HACK: DXIL converts push constant resources into cbuffers without extra annotation, so in the reflection code I
    // have no way of differentiating a normal cbuffer vs a push constant. In DirectX12, I would like to use push
    // constants as root constants, so I need to mark them in some way. Getting push constant info from the spirv
    // reflection and then using that information to differentiate between cbuffers and push constants in spirv.
    std::unordered_set<std::string> push_constant_resources;
    get_push_constant_reflection_info(linked_program, push_constant_resources);

    const std::string_view json_content = std::string_view(
        static_cast<const char*>(json_reflection->getBufferPointer()), json_reflection->getBufferSize());
    nlohmann::json json_reflection_content = nlohmann::json::parse(json_content);

    const std::filesystem::path json_reflection_path = dest_path.string() + ".json";

    for (nlohmann::json& json_resource : json_reflection_content["parameters"])
    {
        const std::string& name = json_resource.value("name", "");

        if (push_constant_resources.contains(name))
        {
            nlohmann::json& json_resource_binding = json_resource["binding"];
            json_resource_binding["kind"] = "pushConstantBuffer";

            nlohmann::json& json_resource_type = json_resource["type"];
            json_resource_type["kind"] = "pushConstantBuffer";
        }
    }

    const std::string& converted_json_content = json_reflection_content.dump(4);
    Filesystem::write_file_string(json_reflection_path, converted_json_content);
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

    slang::CompilerOptionEntry compiler_option_emit_reflection{};
    compiler_option_emit_reflection.name = slang::CompilerOptionName::VulkanEmitReflection;
    compiler_option_emit_reflection.value = slang::CompilerOptionValue{
        .kind = slang::CompilerOptionValueKind::Int,
        .intValue0 = 1,
    };

    std::array compiler_options = {compiler_option_entry_point_name, compiler_option_emit_reflection};

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

void SlangCompiler::get_push_constant_reflection_info(
    const Slang::ComPtr<slang::IComponentType>& program,
    std::unordered_set<std::string>& push_constant_resources) const
{
    Slang::ComPtr<slang::IBlob> diagnostics;

    const uint32_t spirv_target_idx = static_cast<uint32_t>(ShaderBytecodeTarget::Spirv);

    slang::ProgramLayout* spirv_layout = program->getLayout(spirv_target_idx, diagnostics.writeRef());
    diagnose(diagnostics);
    MIZU_ASSERT(spirv_layout != nullptr, "Spirv program layout is nullptr");

    for (uint32_t i = 0; i < spirv_layout->getParameterCount(); ++i)
    {
        slang::VariableLayoutReflection* variable = spirv_layout->getParameterByIndex(i);

        slang::TypeReflection* variable_type = variable->getType();
        slang::TypeLayoutReflection* variable_type_layout = variable->getTypeLayout();

        if (variable_type->getKind() == slang::TypeReflection::Kind::ConstantBuffer
            && variable_type_layout->getParameterCategory() == slang::ParameterCategory::PushConstantBuffer)
        {
            const std::string name = variable->getName();
            push_constant_resources.insert(name);
        }
    }
}

void SlangCompiler::diagnose(const Slang::ComPtr<slang::IBlob>& diagnostics) const
{
    if (diagnostics)
    {
        MIZU_LOG_WARNING("Shader diagnosis: {}", static_cast<const char*>(diagnostics->getBufferPointer()));
    }
}

SlangStage SlangCompiler::mizu_shader_type_to_slang_stage(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return SLANG_STAGE_VERTEX;
    case ShaderType::Fragment:
        return SLANG_STAGE_FRAGMENT;
    case ShaderType::Compute:
        return SLANG_STAGE_COMPUTE;
    case ShaderType::RtxRaygen:
        return SLANG_STAGE_RAY_GENERATION;
    case ShaderType::RtxAnyHit:
        return SLANG_STAGE_ANY_HIT;
    case ShaderType::RtxClosestHit:
        return SLANG_STAGE_CLOSEST_HIT;
    case ShaderType::RtxMiss:
        return SLANG_STAGE_MISS;
    case ShaderType::RtxIntersection:
        return SLANG_STAGE_INTERSECTION;
    }
}

} // namespace Mizu