#include "render/shader/shader_compiler.h"

#include <array>
#include <format>
#include <nlohmann/json.hpp>

#include "base/debug/logging.h"
#include "base/io/filesystem.h"
#include "base/utils/hash.h"
#include "render_core/definitions/shader_types.h"

#include "render/shader/shader_declaration.h"

namespace Mizu
{

#if MIZU_DEBUG

#define SLANG_CHECK(expression)                                    \
    do                                                             \
    {                                                              \
        const SlangResult status = expression;                     \
        MIZU_ASSERT(SLANG_SUCCEEDED(status), "Slang call failed"); \
    } while (false)

#else

#define SLANG_CHECK(expression) expression

#endif

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
    size_t h = 0;
    for (const ShaderCompilationDefine& permutation : m_permutation_values)
    {
        hash_combine(h, permutation.define);
        hash_combine(h, permutation.value);
    }

    return h;
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
    [[maybe_unused]] ShaderType type,
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

    [[maybe_unused]] const SlangStage slang_stage = layout->getEntryPointByIndex(entry_point_idx)->getStage();
    MIZU_ASSERT(
        slang_stage == mizu_shader_type_to_slang_stage(type), "Requested shader type does not match with shader stage");

    Slang::ComPtr<slang::IBlob> bytecode;
    SLANG_CHECK(
        linked_program->getEntryPointCode(entry_point_idx, target_idx, bytecode.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    Filesystem::write_file(
        dest_path, static_cast<const char*>(bytecode->getBufferPointer()), bytecode->getBufferSize());

    // HACK: DXIL converts push constant resources into cbuffers without extra annotation, so in the reflection code I
    // have no way of differentiating a normal cbuffer vs a push constant. In DirectX12, I would like to use push
    // constants as root constants, so I need to mark them in some way. Getting push constant info from the spirv
    // reflection and then using that information to differentiate between cbuffers and push constants in spirv.
    // Also, it seems that push constants usage is not correctly reported by the `isParameterLocationUsed` function
    // (https://github.com/shader-slang/slang/issues/5685), so I have to get the usage of the push constant from the
    // dxil target reflection data (as it is converted to a cbuffer, it doesn't have that problem).
    std::unordered_set<std::string> push_constant_resources;
    get_push_constant_reflection_info(linked_program, push_constant_resources);

    const std::string reflection_info =
        get_reflection_info(linked_program, target_idx, entry_point_idx, push_constant_resources);

    const std::filesystem::path json_reflection_path = dest_path.string() + ".json";
    Filesystem::write_file_string(json_reflection_path, reflection_info);
}

void SlangCompiler::create_session(Slang::ComPtr<slang::ISession>& out_session) const
{
    slang::TargetDesc dxil_target{};
    dxil_target.format = SlangCompileTarget::SLANG_DXIL;
    dxil_target.profile = m_global_session->findProfile("sm_6_6");

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

    slang::CompilerOptionEntry compiler_option_optimization_level{};
    compiler_option_optimization_level.name = slang::CompilerOptionName::Optimization;
    compiler_option_optimization_level.value = slang::CompilerOptionValue{
        .kind = slang::CompilerOptionValueKind::Int,
        .intValue0 = SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_DEFAULT,
    };

    std::array compiler_options = {compiler_option_entry_point_name, compiler_option_optimization_level};

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

std::string SlangCompiler::get_reflection_info(
    const Slang::ComPtr<slang::IComponentType>& program,
    uint32_t target_idx,
    uint32_t entry_point_idx,
    const std::unordered_set<std::string>& push_constant_resources) const
{
    Slang::ComPtr<slang::IBlob> diagnostics;

    Slang::ComPtr<slang::IMetadata> metadata;
    SLANG_CHECK(
        program->getEntryPointMetadata(entry_point_idx, target_idx, metadata.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    slang::ProgramLayout* layout = program->getLayout(target_idx, diagnostics.writeRef());
    diagnose(diagnostics);
    MIZU_ASSERT(layout != nullptr, "Layout is nullptr");

    // Parameters
    std::vector<ShaderResource> parameters;
    std::vector<ShaderPushConstant> constants;

    for (uint32_t parameter_idx = 0; parameter_idx < layout->getParameterCount(); ++parameter_idx)
    {
        slang::VariableLayoutReflection* variable_layout = layout->getParameterByIndex(parameter_idx);
        slang::TypeLayoutReflection* type_layout = variable_layout->getTypeLayout();

        // Special case for ParameterCategory::PushConstantBuffer
        if (variable_layout->getCategory() == slang::ParameterCategory::PushConstantBuffer
            && push_constant_resources.contains(variable_layout->getName()))
        {
            ShaderPushConstant constant{};
            constant.name = variable_layout->getName();
            constant.binding_info = ShaderBindingInfo{};
            constant.size = static_cast<uint32_t>(type_layout->getElementTypeLayout()->getSize());

            constants.push_back(constant);

            continue;
        }

        bool is_parameter_used = false;
        SLANG_CHECK(metadata->isParameterLocationUsed(
            static_cast<SlangParameterCategory>(variable_layout->getCategory()),
            variable_layout->getBindingSpace(),
            variable_layout->getBindingIndex(),
            is_parameter_used));

        if (!is_parameter_used)
            continue;

        slang::VariableReflection* variable = variable_layout->getVariable();
        slang::TypeReflection* variable_type = variable->getType();

        const std::string variable_name = variable_layout->getName();

        ShaderBindingInfo binding_info{};
        binding_info.set = variable_layout->getBindingSpace();
        binding_info.binding = variable_layout->getBindingIndex();

        ShaderResource resource{};
        resource.name = variable_name;
        resource.binding_info = binding_info;

        slang::TypeReflection::Kind variable_kind = variable_type->getKind();
        if (variable_kind == slang::TypeReflection::Kind::SamplerState)
        {
            resource.count = 1;
            resource.value = ShaderResourceSamplerState{};

            parameters.push_back(resource);
        }
        else if (variable_kind == slang::TypeReflection::Kind::ConstantBuffer)
        {
            if (push_constant_resources.contains(variable->getName()))
            {
                ShaderPushConstant constant{};
                constant.name = variable_name;
                constant.binding_info = binding_info;
                constant.size = static_cast<uint32_t>(type_layout->getElementTypeLayout()->getSize());

                constants.push_back(constant);
            }
            else
            {
                ShaderResourceConstantBuffer constant_buffer{};
                constant_buffer.total_size = type_layout->getElementTypeLayout()->getSize();

                slang::TypeLayoutReflection* constant_buffer_layout = type_layout->getElementTypeLayout();
                for (uint32_t field_idx = 0; field_idx < constant_buffer_layout->getFieldCount(); ++field_idx)
                {
                    slang::VariableLayoutReflection* field_variable =
                        constant_buffer_layout->getFieldByIndex(field_idx);

                    const ShaderPrimitive member = get_primitive_reflection(field_variable);
                    constant_buffer.members.push_back(member);
                }

                resource.count = 1;
                resource.value = constant_buffer;

                parameters.push_back(resource);
            }
        }
        else if (variable_kind == slang::TypeReflection::Kind::Resource)
        {
            const SlangResourceShape resource_shape = variable_type->getResourceShape();
            const SlangResourceAccess resource_access = variable_type->getResourceAccess();

            if (resource_shape == SlangResourceShape::SLANG_STRUCTURED_BUFFER)
            {
                ShaderResourceStructuredBuffer structured_buffer{};
                structured_buffer.access = resource_access == SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ
                                               ? ShaderResourceAccessType::ReadOnly
                                               : ShaderResourceAccessType::ReadWrite;

                resource.value = structured_buffer;
            }
            else if (resource_shape == SlangResourceShape::SLANG_BYTE_ADDRESS_BUFFER)
            {
                ShaderResourceByteAddressBuffer byte_address_buffer{};
                byte_address_buffer.access = resource_access == SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ
                                                 ? ShaderResourceAccessType::ReadOnly
                                                 : ShaderResourceAccessType::ReadWrite;

                resource.value = byte_address_buffer;
            }
            else if (resource_shape == SlangResourceShape::SLANG_TEXTURE_2D)
            {
                ShaderResourceTexture texture{};
                texture.access = resource_access == SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ
                                     ? ShaderResourceAccessType::ReadOnly
                                     : ShaderResourceAccessType::ReadWrite;

                resource.value = texture;
            }
            else if (resource_shape == SlangResourceShape::SLANG_TEXTURE_CUBE)
            {
                resource.value = ShaderResourceTextureCube{};
            }
            else if (resource_shape == SlangResourceShape::SLANG_ACCELERATION_STRUCTURE)
            {
                resource.value = ShaderResourceAccelerationStructure{};
            }
            else
            {
                MIZU_UNREACHABLE("Invalid resource shape");
            }

            resource.count = 1;

            parameters.push_back(resource);
        }
        else if (variable_kind == slang::TypeReflection::Kind::Array)
        {
            const SlangResourceShape resource_shape = variable_type->getResourceShape();
            const SlangResourceAccess resource_access = variable_type->getResourceAccess();

            if (resource_shape == SlangResourceShape::SLANG_TEXTURE_2D)
            {
                ShaderResourceTexture texture{};
                texture.access = resource_access == SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ
                                     ? ShaderResourceAccessType::ReadOnly
                                     : ShaderResourceAccessType::ReadWrite;

                resource.value = texture;
            }
            else
            {
                MIZU_UNREACHABLE("Invalid resource shape");
            }

            resource.count = static_cast<uint32_t>(variable_type->getElementCount());

            parameters.push_back(resource);
        }
        else
        {
            MIZU_UNREACHABLE("Variable kind is invalid or not implemented");
        }
    }

    // Inputs & Outputs
    slang::EntryPointReflection* entry_point_reflection = layout->getEntryPointByIndex(entry_point_idx);

    const auto get_input_output_reflection_info = [this](
                                                      slang::VariableLayoutReflection* layout,
                                                      std::vector<ShaderInputOutput>& out_vector,
                                                      slang::ParameterCategory category) {
        if (layout->getTypeLayout()->getKind() == slang::TypeReflection::Kind::Struct)
        {
            const uint32_t struct_field_count = layout->getTypeLayout()->getFieldCount();
            for (uint32_t struct_input_idx = 0; struct_input_idx < struct_field_count; ++struct_input_idx)
            {
                slang::VariableLayoutReflection* struct_field =
                    layout->getTypeLayout()->getFieldByIndex(struct_input_idx);

                ShaderInputOutput input_output{};
                input_output.location = struct_field->getBindingIndex();
                input_output.primitive = get_primitive_reflection(struct_field);

                if (struct_field->getSemanticName() != nullptr)
                {
                    input_output.semantic_name = struct_field->getSemanticName();
                    input_output.semantic_index = static_cast<uint32_t>(struct_field->getSemanticIndex());
                }

                out_vector.push_back(input_output);
            }
        }
        else if (layout->getTypeLayout()->getParameterCategory() == category)
        {
            ShaderInputOutput input_output{};
            input_output.location = layout->getBindingIndex();
            input_output.primitive = get_primitive_reflection(layout);

            if (layout->getSemanticName() != nullptr)
            {
                input_output.semantic_name = std::format("{}{}", layout->getSemanticName(), layout->getSemanticIndex());
            }

            out_vector.push_back(input_output);
        }
    };

    // Inputs
    std::vector<ShaderInputOutput> inputs;

    slang::TypeLayoutReflection* input_type_layout = entry_point_reflection->getTypeLayout();
    MIZU_ASSERT(input_type_layout->getKind() == slang::TypeReflection::Kind::Struct, "Input type is not a struct");

    const uint32_t input_field_count = input_type_layout->getFieldCount();
    for (uint32_t input_idx = 0; input_idx < input_field_count; ++input_idx)
    {
        slang::VariableLayoutReflection* input_field = input_type_layout->getFieldByIndex(input_idx);
        get_input_output_reflection_info(input_field, inputs, slang::ParameterCategory::VaryingInput);
    }

    // Outputs
    std::vector<ShaderInputOutput> outputs;
    get_input_output_reflection_info(
        entry_point_reflection->getResultVarLayout(), outputs, slang::ParameterCategory::VaryingOutput);

    // Convert into json
    nlohmann::json output_json;

    constexpr const char* RESOURCE_TYPE_KEY = "resource_type";
    constexpr const char* ACCESS_TYPE_KEY = "access_type";
    constexpr const char* BINDING_INFO_KEY = "binding_info";

    nlohmann::json json_parameters;
    for (const ShaderResource& resource : parameters)
    {
        nlohmann::json json_parameter;
        json_parameter["name"] = resource.name;

        json_parameter[BINDING_INFO_KEY] = {
            {"set", resource.binding_info.set},
            {"binding", resource.binding_info.binding},
        };

        json_parameter["count"] = resource.count;

        if (std::holds_alternative<ShaderResourceTexture>(resource.value))
        {
            const ShaderResourceTexture& texture = std::get<ShaderResourceTexture>(resource.value);

            json_parameter[RESOURCE_TYPE_KEY] = "texture";
            json_parameter[ACCESS_TYPE_KEY] = static_cast<uint32_t>(texture.access);
        }
        else if (std::holds_alternative<ShaderResourceTextureCube>(resource.value))
        {
            json_parameter[RESOURCE_TYPE_KEY] = "texture_cube";
        }
        else if (std::holds_alternative<ShaderResourceStructuredBuffer>(resource.value))
        {
            const ShaderResourceStructuredBuffer& structured_buffer =
                std::get<ShaderResourceStructuredBuffer>(resource.value);

            json_parameter[RESOURCE_TYPE_KEY] = "structured_buffer";
            json_parameter[ACCESS_TYPE_KEY] = static_cast<uint32_t>(structured_buffer.access);
        }
        else if (std::holds_alternative<ShaderResourceByteAddressBuffer>(resource.value))
        {
            const ShaderResourceByteAddressBuffer& structured_buffer =
                std::get<ShaderResourceByteAddressBuffer>(resource.value);

            json_parameter[RESOURCE_TYPE_KEY] = "byte_address_buffer";
            json_parameter[ACCESS_TYPE_KEY] = static_cast<uint32_t>(structured_buffer.access);
        }
        else if (std::holds_alternative<ShaderResourceConstantBuffer>(resource.value))
        {
            const ShaderResourceConstantBuffer& constant_buffer =
                std::get<ShaderResourceConstantBuffer>(resource.value);

            json_parameter[RESOURCE_TYPE_KEY] = "constant_buffer";
            json_parameter["total_size"] = constant_buffer.total_size;

            nlohmann::json json_members;
            for (const ShaderPrimitive& member : constant_buffer.members)
            {
                nlohmann::json json_member;
                json_member["name"] = member.name;
                json_member["type"] = static_cast<uint32_t>(member.type);

                json_members.push_back(json_member);
            }

            json_parameter["members"] = json_members;
        }
        else if (std::holds_alternative<ShaderResourceSamplerState>(resource.value))
        {
            json_parameter[RESOURCE_TYPE_KEY] = "sampler_state";
        }
        else if (std::holds_alternative<ShaderResourceAccelerationStructure>(resource.value))
        {
            json_parameter[RESOURCE_TYPE_KEY] = "acceleration_structure";
        }
        else
        {
            MIZU_UNREACHABLE("Not implemented or invalid type");
        }

        json_parameters.push_back(json_parameter);
    }

    nlohmann::json json_push_constants;
    for (const ShaderPushConstant& constant : constants)
    {
        nlohmann::json json_push_constant;

        json_push_constant["name"] = constant.name;
        json_push_constant["binding_info"] = {
            {"set", constant.binding_info.set},
            {"binding", constant.binding_info.binding},
        };
        json_push_constant["size"] = constant.size;

        json_push_constants.push_back(json_push_constant);
    }

    nlohmann::json json_inputs;
    for (const ShaderInputOutput& input : inputs)
    {
        nlohmann::json json_input;

        json_input["semantic_name"] = input.semantic_name;
        json_input["semantic_index"] = input.semantic_index;
        json_input["location"] = input.location;
        json_input["primitive"] = {
            {"name", input.primitive.name}, {"type", static_cast<uint32_t>(input.primitive.type)}};

        json_inputs.push_back(json_input);
    }

    nlohmann::json json_outputs;
    for (const ShaderInputOutput& output : outputs)
    {
        nlohmann::json json_output;

        json_output["semantic_name"] = output.semantic_name;
        json_output["semantic_index"] = output.semantic_index;
        json_output["location"] = output.location;
        json_output["primitive"] = {
            {"name", output.primitive.name}, {"type", static_cast<uint32_t>(output.primitive.type)}};

        json_outputs.push_back(json_output);
    }

    output_json["parameters"] = json_parameters;
    output_json["push_constants"] = json_push_constants;
    output_json["inputs"] = json_inputs;
    output_json["outputs"] = json_outputs;

    return output_json.dump(4);
}

void SlangCompiler::get_push_constant_reflection_info(
    const Slang::ComPtr<slang::IComponentType>& program,
    std::unordered_set<std::string>& push_constant_resources) const
{
    Slang::ComPtr<slang::IBlob> diagnostics;

    const uint32_t dxil_target_idx = static_cast<uint32_t>(ShaderBytecodeTarget::Dxil);
    const uint32_t spirv_target_idx = static_cast<uint32_t>(ShaderBytecodeTarget::Spirv);

    slang::ProgramLayout* dxil_layout = program->getLayout(dxil_target_idx, diagnostics.writeRef());
    diagnose(diagnostics);
    MIZU_ASSERT(dxil_layout != nullptr, "Dxil program layout is nullptr");

    slang::ProgramLayout* spirv_layout = program->getLayout(spirv_target_idx, diagnostics.writeRef());
    diagnose(diagnostics);
    MIZU_ASSERT(spirv_layout != nullptr, "Spirv program layout is nullptr");

    Slang::ComPtr<slang::IMetadata> dxil_metadata;
    SLANG_CHECK(program->getEntryPointMetadata(0, dxil_target_idx, dxil_metadata.writeRef(), diagnostics.writeRef()));
    diagnose(diagnostics);

    for (uint32_t i = 0; i < spirv_layout->getParameterCount(); ++i)
    {
        slang::VariableLayoutReflection* variable_layout = spirv_layout->getParameterByIndex(i);
        slang::VariableReflection* variable = variable_layout->getVariable();

        if (variable_layout->getCategory() == slang::ParameterCategory::PushConstantBuffer)
        {
            slang::VariableLayoutReflection* dxil_variable_layout = dxil_layout->getParameterByIndex(i);

            bool is_push_constant_used;
            SLANG_CHECK(dxil_metadata->isParameterLocationUsed(
                static_cast<SlangParameterCategory>(dxil_variable_layout->getCategory()),
                dxil_variable_layout->getBindingSpace(),
                dxil_variable_layout->getBindingIndex(),
                is_push_constant_used));

            if (is_push_constant_used)
            {
                const std::string name = variable->getName();
                push_constant_resources.insert(name);
            }
        }
    }
}

ShaderPrimitive SlangCompiler::get_primitive_reflection(slang::VariableLayoutReflection* layout) const
{
    ShaderPrimitive primitive{};
    primitive.name = layout->getName() != nullptr ? layout->getName() : "";
    primitive.type = get_primitive_type_reflection(layout->getTypeLayout());

    return primitive;
}

ShaderPrimitiveType SlangCompiler::get_primitive_type_reflection(slang::TypeLayoutReflection* layout) const
{
    const slang::TypeReflection::Kind kind = layout->getKind();
    const slang::TypeReflection::ScalarType scalar_type = layout->getScalarType();

    const uint32_t row_count = layout->getRowCount();
    const uint32_t col_count = layout->getColumnCount();

    if (kind == slang::TypeReflection::Kind::Scalar)
    {
        if (scalar_type == slang::TypeReflection::ScalarType::Bool)
        {
            return ShaderPrimitiveType::Bool;
        }
        else if (scalar_type == slang::TypeReflection::ScalarType::Float32)
        {
            return ShaderPrimitiveType::Float;
        }
        else if (scalar_type == slang::TypeReflection::ScalarType::UInt32)
        {
            return ShaderPrimitiveType::UInt;
        }
        else if (scalar_type == slang::TypeReflection::ScalarType::UInt64)
        {
            return ShaderPrimitiveType::ULong;
        }
    }
    else if (kind == slang::TypeReflection::Kind::Vector)
    {
        if (scalar_type == slang::TypeReflection::ScalarType::Float32)
        {
            // clang-format off
            if (col_count == 2)      return ShaderPrimitiveType::Float2;
            else if (col_count == 3) return ShaderPrimitiveType::Float3;
            else if (col_count == 4) return ShaderPrimitiveType::Float4;
            // clang-format on
        }
        else if (scalar_type == slang::TypeReflection::ScalarType::UInt32)
        {
            // clang-format off
            if (col_count == 2)      return ShaderPrimitiveType::UInt2;
            else if (col_count == 3) return ShaderPrimitiveType::UInt3;
            else if (col_count == 4) return ShaderPrimitiveType::UInt4;
            // clang-format on
        }
    }
    else if (kind == slang::TypeReflection::Kind::Matrix)
    {
        if (scalar_type == slang::TypeReflection::ScalarType::Float32)
        {
            // clang-format off
            if (col_count == 3 && row_count == 3)      return ShaderPrimitiveType::Float3x3;
            else if (col_count == 4 && row_count == 4) return ShaderPrimitiveType::Float4x4;
            // clang-format on
        }
    }

    MIZU_UNREACHABLE("Not implemented type");

    return ShaderPrimitiveType::Float; // Default return to prevent compilation errors
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