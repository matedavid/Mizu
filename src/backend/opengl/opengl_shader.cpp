#include "opengl_shader.h"

#include <algorithm>
#include <numeric>
#include <ranges>

#include "utility/filesystem.h"
#include "utility/logging.h"

namespace Mizu::OpenGL {

//
// OpenGLShaderBase
//

OpenGLShaderBase::~OpenGLShaderBase() {
    glDeleteProgram(m_program);
}

GLuint OpenGLShaderBase::compile_shader(GLenum type, const char* src, const std::filesystem::path& path) {
    const GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        MIZU_LOG_ERROR("OpenGL shader compilation error: {}", path.string());
        MIZU_LOG_ERROR("  {}", infoLog);

        assert(false);
    }

    return shader;
}

std::vector<ShaderProperty> OpenGLShaderBase::get_properties_internal() const {
    std::vector<ShaderProperty> properties;
    for (const auto& prop : m_uniforms | std::views::values)
        properties.push_back(prop);

    return properties;
}

std::optional<ShaderProperty> OpenGLShaderBase::get_property_internal(std::string_view name) const {
    const auto it = m_uniforms.find(std::string{name});
    if (it == m_uniforms.end())
        return std::nullopt;

    return it->second;
}

std::optional<ShaderConstant> OpenGLShaderBase::get_constant_internal(std::string_view name) const {
    // TODO:
}

void OpenGLShaderBase::retrieve_uniforms_info() {
    int32_t name_max_length;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &name_max_length);

    char glsl_name[512];

    int32_t uniform_count;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &uniform_count);

    for (uint32_t i = 0; i < static_cast<uint32_t>(uniform_count); i++) {
        GLint size;
        GLenum type;

        glGetActiveUniform(m_program, i, name_max_length, NULL, &size, &type, glsl_name);

        const auto uniform_name = std::string{glsl_name};

        // If uniform has dot, consider that is part of a Uniform Buffer
        const auto pos = uniform_name.find('.');
        if (pos != std::string::npos) {
            const auto uniform_buffer_name = uniform_name.substr(0, pos);
            const auto member_name = uniform_name.substr(pos + 1, uniform_name.size() - 1);

            auto ub_it = m_uniforms.find(uniform_buffer_name);
            if (ub_it == m_uniforms.end()) {
                ShaderUniformBufferProperty prop{};
                prop.name = uniform_buffer_name;
                // TODO: Calculate total_size
                prop.members = std::vector<ShaderValueProperty>{};

                ub_it = m_uniforms.insert({uniform_buffer_name, prop}).first;
            }

            if (type == GL_SAMPLER_2D) {
                MIZU_LOG_ERROR("sampler2D inside struct is not supported (member: {})", uniform_name);
                continue;
            }

            const auto [type_, size_] = get_uniform_info(type);

            ShaderValueProperty value{};
            value.name = member_name;
            value.type = type_;
            value.size = size_;

            auto& ubo = std::get<ShaderUniformBufferProperty>(ub_it->second);
            ubo.members.push_back(value);
        } else if (type == GL_SAMPLER_2D) {
            ShaderTextureProperty prop{};
            prop.name = uniform_name;

            m_uniforms.insert({uniform_name, prop});
        } else {
            const auto [type_, size_] = get_uniform_info(type);

            ShaderValueProperty prop{};
            prop.name = uniform_name;
            prop.type = type_;
            prop.size = size_;
        }
    }

    // Compute total size in uniform buffer properties
    for (auto& prop : m_uniforms | std::views::values) {
        if (!std::holds_alternative<ShaderUniformBufferProperty>(prop))
            continue;

        auto& ub = std::get<ShaderUniformBufferProperty>(prop);
        ub.total_size = std::accumulate(ub.members.begin(),
                                        ub.members.end(),
                                        uint32_t(),
                                        [](uint32_t accum, ShaderValueProperty val) { return accum + val.size; });
    }
}

std::pair<ShaderValueProperty::Type, uint32_t> OpenGLShaderBase::get_uniform_info(GLenum type) {
    switch (type) {
    case GL_FLOAT_VEC2:
        return {ShaderValueProperty::Type::Vec2, sizeof(glm::vec2)};
    case GL_FLOAT_VEC3:
        return {ShaderValueProperty::Type::Vec3, sizeof(glm::vec3)};
    case GL_FLOAT_VEC4:
        return {ShaderValueProperty::Type::Vec4, sizeof(glm::vec4)};
    case GL_FLOAT_MAT3:
        return {ShaderValueProperty::Type::Mat3, sizeof(glm::mat3)};
    case GL_FLOAT_MAT4:
        return {ShaderValueProperty::Type::Mat4, sizeof(glm::mat4)};
    default:
        assert(false && "Uniform type not recognized");
    }
}

//
// OpenGLShader
//

OpenGLShader::OpenGLShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path) {
    const auto vertex_src = Filesystem::read_file(vertex_path);
    const auto fragment_src = Filesystem::read_file(fragment_path);

    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src.data(), vertex_path);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src.data(), fragment_path);

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex_shader);
    glAttachShader(m_program, fragment_shader);
    glLinkProgram(m_program);

    // TODO: Check linking success?

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    retrieve_uniforms_info();
}

std::vector<ShaderProperty> OpenGLShader::get_properties() const {
    return get_properties_internal();
}

std::optional<ShaderProperty> OpenGLShader::get_property(std::string_view name) const {
    return get_property_internal(name);
}

std::optional<ShaderConstant> OpenGLShader::get_constant(std::string_view name) const {
    return get_constant_internal(name);
}

} // namespace Mizu::OpenGL