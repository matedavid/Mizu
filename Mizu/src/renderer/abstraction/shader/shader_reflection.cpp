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

    // input variables
    std::ranges::sort(properties.stage_inputs, [&](spirv_cross::Resource& a, spirv_cross::Resource& b) {
        return glsl.get_decoration(a.id, spv::DecorationLocation) < glsl.get_decoration(b.id, spv::DecorationLocation);
    });

    m_inputs.reserve(properties.stage_inputs.size());
    for (const spirv_cross::Resource& input : properties.stage_inputs) {
        const ShaderType type = spirv_internal_to_type(glsl.get_type(input.type_id));

        m_inputs.push_back({input.name, type});
    }
}

} // namespace Mizu