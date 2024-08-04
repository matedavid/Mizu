#include "shader_reflection.h"

#include <spirv_glsl.hpp>

#include "utility/assert.h"

namespace Mizu {

static ShaderType spirv_internal_to_type(spirv_cross::SPIRType type) {
    using Type = spirv_cross::SPIRType;

    if (type.type == Type::Float) {
        return ShaderType::Float;
    } else if (type.columns == 1) {
        switch (type.vecsize) {
        case 2:
            return ShaderType::Vec2;
        case 3:
            return ShaderType::Vec3;
        case 4:
            return ShaderType::Vec4;
        }
    } else if (type.columns == 3) {
        return ShaderType::Mat3;
    } else if (type.columns == 4) {
        return ShaderType::Mat4;
    }

    MIZU_VERIFY(false, "Spirv-cross type not recognized");
}

ShaderReflection::ShaderReflection(const std::vector<char>& source) {
    const auto* data = reinterpret_cast<const uint32_t*>(source.data());

    const spirv_cross::CompilerGLSL glsl(data, source.size() / sizeof(uint32_t));

    auto properties = glsl.get_shader_resources();

    // inputs
    std::ranges::sort(properties.stage_inputs, [&](spirv_cross::Resource& a, spirv_cross::Resource& b) {
        return glsl.get_decoration(a.id, spv::DecorationLocation) < glsl.get_decoration(b.id, spv::DecorationLocation);
    });

    m_inputs.reserve(properties.stage_inputs.size());
    for (const auto& input : properties.stage_inputs) {
        const ShaderType type = spirv_internal_to_type(glsl.get_type(input.type_id));
        const uint32_t location = glsl.get_decoration(input.id, spv::DecorationLocation);

        m_inputs.push_back({input.name, type, location});
    }

    // properties

    {
        // image properties
        const auto add_texture_properties = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& properties,
                                                ShaderTextureProperty2::Type type) {
            for (const auto& resource : properties) {
                ShaderTextureProperty2 value;
                value.type = type;

                ShaderProperty2 property;
                property.name = resource.name;
                property.value = value;
                property.binding_info.set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
                property.binding_info.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

                m_properties.push_back(property);
            }
        };

        add_texture_properties(properties.sampled_images, ShaderTextureProperty2::Type::Sampled);
        add_texture_properties(properties.separate_images, ShaderTextureProperty2::Type::Separate);
        add_texture_properties(properties.storage_images, ShaderTextureProperty2::Type::Storage);
    }

    {
        // buffer properties
        const auto add_buffer_properties = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& properties,
                                               ShaderBufferProperty2::Type type) {
            for (const auto& resource : properties) {
                ShaderBufferProperty2 value;
                value.type = type;
                value.total_size = glsl.get_declared_struct_size(glsl.get_type(resource.base_type_id));

                const size_t num_members = glsl.get_type(resource.base_type_id).member_types.size();

                value.members.reserve(num_members);

                for (size_t i = 0; i < num_members; ++i) {
                    const std::string member_name = glsl.get_member_name(resource.base_type_id, i);
                    const spirv_cross::SPIRType member_type =
                        glsl.get_type(glsl.get_type(resource.base_type_id).member_types[i]);

                    ShaderMemberProperty2 member;
                    member.name = member_name;
                    member.type = spirv_internal_to_type(member_type);

                    value.members.push_back(member);
                }

                /*
                By default spirv_cross returns the Uniform Buffer name, not the variable name. We want the variable name
                if it's available. For example:

                uniform Buffer {
                    mat4 x;
                } uBuffer;

                Will return 'Buffer' in resource.name. To get 'uBuffer' we need to use glsl.get_name(resource.id).
                */
                const std::string id_name = glsl.get_name(resource.id);

                ShaderProperty2 property;
                property.name = id_name.empty() ? resource.name : id_name;
                property.value = value;
                property.binding_info.set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
                property.binding_info.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

                m_properties.push_back(property);
            }
        };

        add_buffer_properties(properties.uniform_buffers, ShaderBufferProperty2::Type::Uniform);
        add_buffer_properties(properties.storage_buffers, ShaderBufferProperty2::Type::Storage);
    }

    // constants
    for (const auto& push_constant : properties.push_constant_buffers) {
        ShaderConstant2 constant;
        constant.name = push_constant.name;
        constant.size = glsl.get_declared_struct_size(glsl.get_type(push_constant.base_type_id));

        m_constants.push_back(constant);
    }
}

} // namespace Mizu