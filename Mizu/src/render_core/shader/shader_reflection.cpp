#include "shader_reflection.h"

#include <optional>
#include <spirv_glsl.hpp>

#include "utility/assert.h"

namespace Mizu
{

static std::optional<ShaderType> spirv_internal_to_type_scalar(const spirv_cross::SPIRType::BaseType btype)
{
    // TODO: Implement rest of scalar types
    switch (btype)
    {
    case spirv_cross::SPIRType::Boolean:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::SByte:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::UByte:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Short:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::UShort:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Int:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::UInt:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Int64:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::UInt64:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::AtomicCounter:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Half:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Float:
        return ShaderType::Float;
    case spirv_cross::SPIRType::Double:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Char:
        MIZU_UNREACHABLE("TODO");
    case spirv_cross::SPIRType::Unknown:
    case spirv_cross::SPIRType::Void:
    case spirv_cross::SPIRType::Struct:
    case spirv_cross::SPIRType::Image:
    case spirv_cross::SPIRType::SampledImage:
    case spirv_cross::SPIRType::Sampler:
    case spirv_cross::SPIRType::AccelerationStructure:
    case spirv_cross::SPIRType::RayQuery:
    case spirv_cross::SPIRType::ControlPointArray:
    case spirv_cross::SPIRType::Interpolant:
        return std::nullopt;
    }
}

static ShaderType create_vector_shader_type(const ShaderType& btype, uint32_t components)
{
    MIZU_ASSERT(ShaderType::is_scalar(btype), "Base type provided is not scalar");
    MIZU_ASSERT(components >= 2 && components <= 4, "Number of components not supported");

    if (btype == ShaderType::Float)
    {
        if (components == 2)
            return ShaderType::Float2;
        if (components == 3)
            return ShaderType::Float3;
        if (components == 4)
            return ShaderType::Float4;
    }

    // TODO: Implement rest of scalar types

    MIZU_UNREACHABLE("Unimplemented");
}

static ShaderType create_matrix_shader_type(const ShaderType& btype, uint32_t rows, uint32_t cols)
{
    MIZU_ASSERT(ShaderType::is_scalar(btype), "Base type provided is not scalar");
    MIZU_ASSERT(rows >= 2 && rows <= 4, "Number of rows not supported");
    MIZU_ASSERT(cols >= 2 && cols <= 4, "Number of cols not supported");

    // clang-format off
    if (btype == ShaderType::Float) {
        if (rows == 2) {
            // if (cols == 2) return ShaderType::Float2x2;
            // if (cols == 3) return ShaderType::Float2x3;
            // if (cols == 4) return ShaderType::Float2x4;
        } else if (rows == 3) {
            // if (cols == 2) return ShaderType::Float3x2;
            if (cols == 3) return ShaderType::Float3x3;
            // if (cols == 4) return ShaderType::Float3x4;
        } else if (rows == 4) {
            // if (cols == 2) return ShaderType::Float4x2;
            // if (cols == 3) return ShaderType::Float4x3;
             if (cols == 4) return ShaderType::Float4x4;
        }
    }
    // clang-format on

    // TODO: Implement rest of scalar types and matrix types

    MIZU_UNREACHABLE("Unimplemented");
}

static ShaderType spirv_internal_to_type(const spirv_cross::CompilerGLSL& glsl, const spirv_cross::SPIRType& type)
{
    using Type = spirv_cross::SPIRType;

    const std::optional<ShaderType> scalar_type = spirv_internal_to_type_scalar(type.basetype);

    // 1. Check if it's scalar (float, double...)
    if (type.columns == 1 && type.vecsize == 1 && scalar_type.has_value())
    {
        return *scalar_type;
    }

    // 2. Check if it's vector (float2, float4, double2...)
    if (type.columns == 1 && (type.vecsize >= 2 && type.vecsize <= 4) && scalar_type.has_value())
    {
        return create_vector_shader_type(*scalar_type, type.vecsize);
    }

    // 3. Check if it's matrix type (float4x4, float2x4...)
    // TODO: This only supports row major matrices
    if (type.basetype == Type::Struct && type.member_types.size() == 1)
    {
        // 3.1. Alternative representation, emitted by slang compiler
        //      In this case, the type is represented as a struct with a single `member_type`.
        //      This type has rows = `vecsize`, and columns = the first element of `array`.
        const spirv_cross::SPIRType& child_type = glsl.get_type(type.member_types[0]);

        const std::optional<ShaderType>& child_scalar_type = spirv_internal_to_type_scalar(child_type.basetype);
        if (child_scalar_type.has_value() && child_type.columns == 1 && child_type.array.size() == 1
            && (child_type.vecsize >= 2 && child_type.vecsize <= 4))
        {
            return create_matrix_shader_type(*child_scalar_type, child_type.vecsize, child_type.array[0]);
        }
    }
    else if ((type.columns >= 2 && type.columns <= 4) && (type.vecsize >= 2 && type.vecsize <= 4))
    {
        // 3.2. Common representation
        return create_matrix_shader_type(*scalar_type, type.vecsize, type.columns);
    }

    MIZU_UNREACHABLE("Spirv-cross type not recognized");
}

ShaderReflection::ShaderReflection(const std::vector<char>& source)
{
    const auto* data = reinterpret_cast<const uint32_t*>(source.data());

    const spirv_cross::CompilerGLSL glsl(data, source.size() / sizeof(uint32_t));

    auto properties = glsl.get_shader_resources();

    // inputs
    std::ranges::sort(properties.stage_inputs, [&](spirv_cross::Resource& a, spirv_cross::Resource& b) {
        return glsl.get_decoration(a.id, spv::DecorationLocation) < glsl.get_decoration(b.id, spv::DecorationLocation);
    });

    m_inputs.reserve(properties.stage_inputs.size());
    for (const auto& input : properties.stage_inputs)
    {
        const ShaderType type = spirv_internal_to_type(glsl, glsl.get_type(input.type_id));
        const uint32_t location = glsl.get_decoration(input.id, spv::DecorationLocation);

        m_inputs.push_back({input.name, type, location});
    }

    // properties

    {
        // image properties
        const auto add_texture_properties = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& properties,
                                                ShaderTextureProperty::Type type) {
            for (const auto& resource : properties)
            {
                ShaderTextureProperty value{};
                value.type = type;

                ShaderProperty property;
                property.name = resource.name;
                property.value = value;
                property.binding_info.set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
                property.binding_info.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

                m_properties.push_back(property);
            }
        };

        add_texture_properties(properties.sampled_images, ShaderTextureProperty::Type::Sampled);
        // TODO: For the moment, separte images will be treated as combined image samplers. This will come back to bite
        // me once Directx12 is being implemented :)
        add_texture_properties(properties.separate_images, ShaderTextureProperty::Type::Sampled);
        add_texture_properties(properties.storage_images, ShaderTextureProperty::Type::Storage);
    }

    {
        // buffer properties
        const auto add_buffer_properties = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& properties,
                                               ShaderBufferProperty::Type type) {
            for (const auto& resource : properties)
            {
                ShaderBufferProperty value;
                value.type = type;
                // value.total_size = glsl.get_declared_struct_size(glsl.get_type(resource.base_type_id));

                const size_t num_members = glsl.get_type(resource.base_type_id).member_types.size();

                value.members.reserve(num_members);

                uint32_t total_padded_size = 0;
                for (size_t i = 0; i < num_members; ++i)
                {
                    const std::string& member_name = glsl.get_member_name(resource.base_type_id, i);
                    const spirv_cross::SPIRType& member_type =
                        glsl.get_type(glsl.get_type(resource.base_type_id).member_types[i]);

                    ShaderMemberProperty member;
                    member.name = member_name;
                    member.type = spirv_internal_to_type(glsl, member_type);

                    value.members.push_back(member);

                    total_padded_size += ShaderType::padded_size(member.type);
                }

                value.total_size = total_padded_size;

                /*
                By default spirv_cross returns the Uniform Buffer name, not the variable name. We want the variable name
                if it's available. For example:

                uniform Buffer {
                    mat4 x;
                } uBuffer;

                Will return 'Buffer' in resource.name. To get 'uBuffer' we need to use glsl.get_name(resource.id).
                */
                const std::string& id_name = glsl.get_name(resource.id);

                ShaderProperty property;
                property.name = id_name.empty() ? resource.name : id_name;
                property.value = value;
                property.binding_info.set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
                property.binding_info.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

                m_properties.push_back(property);
            }
        };

        add_buffer_properties(properties.uniform_buffers, ShaderBufferProperty::Type::Uniform);
        add_buffer_properties(properties.storage_buffers, ShaderBufferProperty::Type::Storage);
    }

    // constants
    for (const auto& push_constant : properties.push_constant_buffers)
    {
        ShaderConstant constant;
        constant.name = push_constant.name;
        constant.size = static_cast<uint32_t>(glsl.get_declared_struct_size(glsl.get_type(push_constant.base_type_id)));

        m_constants.push_back(constant);
    }
}

} // namespace Mizu