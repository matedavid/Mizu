#include "opengl_shader.h"

#include <ranges>

#include "utility/assert.h"
#include "utility/filesystem.h"
#include "utility/logging.h"

#include "renderer/abstraction/shader/shader_transpiler.h"

namespace Mizu::OpenGL {

//
// OpenGLShaderBase
//

OpenGLShaderBase::~OpenGLShaderBase() {
    glDeleteProgram(m_program);
}

std::optional<OpenGLUniformInfo> OpenGLShaderBase::get_uniform_info(std::string_view name) const {
    const auto it = m_uniform_info.find(std::string{name});
    if (it == m_uniform_info.end())
        return std::nullopt;

    return it->second;
}

GLuint OpenGLShaderBase::compile_shader(GLenum type, const std::filesystem::path& path) {
    if (path.extension() != ".spv") {
        MIZU_LOG_WARNING("OpenGL shader extension is not .spv, are you sure is a spir-v shader? At the moment OpenGL "
                         "implementation only supports spir-v shaders.");
    }

    const auto spirv_source = Filesystem::read_file(path);

    const auto compiler = ShaderTranspiler{spirv_source, ShaderTranspiler::Translation::Vulkan_2_OpenGL46};
    const auto glsl_source = compiler.compile();

    const GLuint shader = glCreateShader(type);

    const auto* c_src = glsl_source.data();
    glShaderSource(shader, 1, &c_src, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        MIZU_LOG_ERROR("OpenGL shader compilation error: {}", path.string());
        MIZU_LOG_ERROR("  {}", infoLog);

        MIZU_ASSERT(false, "Failed to compile shader");
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
    const auto prop = get_property_internal(name);
    if (!prop.has_value())
        return std::nullopt;

    MIZU_ASSERT(std::holds_alternative<ShaderUniformBufferProperty>(prop->value),
                "Constant should be a Uniform Buffer");

    const auto val = std::get<ShaderUniformBufferProperty>(prop->value);

    ShaderConstant constant{};
    constant.name = prop->name;
    constant.size = val.total_size;

    return constant;
}

void OpenGLShaderBase::retrieve_uniforms_info() {
    int32_t name_max_length;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &name_max_length);

    char glsl_name[512];

    int32_t uniform_count;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &uniform_count);

    std::unordered_map<std::string, uint32_t> ubo_total_size;

    for (uint32_t i = 0; i < static_cast<uint32_t>(uniform_count); i++) {
        GLint size;
        GLenum type;
        glGetActiveUniform(m_program, i, name_max_length, NULL, &size, &type, glsl_name);

        const auto uniform_name = std::string{glsl_name};

        // If uniform has dot, consider that is part of a Uniform Buffer
        const auto pos = uniform_name.find('.');
        if (pos != std::string::npos) {
            /*
             * When reading a UBO from opengl, if we have the following uniform buffer declaration:
             *
             * layout (binding = x) uniform BufferInfo {
             *      vec4 color;
             * } uBufferInfo;
             *
             * We will get the following uniform_name: 'BufferInfo.color' (for each member of the struct).
             */

            auto uniform_buffer_name = uniform_name.substr(0, pos);
            auto member_name = uniform_name.substr(pos + 1, uniform_name.size() - 1);

            const auto [type_, size_, padded_size] = get_uniform_info(type);

            auto ub_it = m_uniforms.find(uniform_buffer_name);
            auto total_size_it = ubo_total_size.find(uniform_buffer_name);
            if (ub_it == m_uniforms.end()) {
                ShaderUniformBufferProperty value{};
                value.members = std::vector<ShaderValueProperty>{};

                ShaderProperty property;
                property.name = uniform_buffer_name;
                property.set = 0;
                property.value = value;

                ub_it = m_uniforms.insert({uniform_buffer_name, property}).first;
                total_size_it = ubo_total_size.insert({uniform_buffer_name, 0}).first;

                GLuint block_binding = glGetUniformBlockIndex(m_program, uniform_buffer_name.c_str());

                OpenGLUniformInfo info{};
                info.type = OpenGLUniformType::UniformBuffer;
                info.binding = static_cast<uint32_t>(block_binding);

                m_uniform_info.insert({uniform_buffer_name, info});
            }

            if (type == GL_SAMPLER_2D || type == GL_IMAGE_2D) {
                MIZU_LOG_ERROR("sampler2D inside struct is not supported (member: {})", uniform_name);
                continue;
            }

            ShaderValueProperty value{};
            value.name = member_name;
            value.type = type_;
            value.size = size_;

            auto& ubo = std::get<ShaderUniformBufferProperty>(ub_it->second.value);
            ubo.members.push_back(value);

            total_size_it->second += padded_size;
        } else if (type == GL_SAMPLER_2D || type == GL_IMAGE_2D) {
            ShaderProperty property;
            property.name = uniform_name;
            property.set = 0;
            property.value = ShaderTextureProperty{};

            m_uniforms.insert({uniform_name, property});

            OpenGLUniformInfo info{};
            info.type = OpenGLUniformType::Texture;
            info.binding = i;
            info.location = glGetUniformLocation(m_program, uniform_name.c_str());
            assert(info.location != -1 && "Invalid location for texture");

            m_uniform_info.insert({uniform_name, info});
        } else {
            const auto [type_, size_, _] = get_uniform_info(type);

            ShaderValueProperty prop{};
            prop.name = uniform_name;
            prop.type = type_;
            prop.size = size_;
        }
    }

    // Compute total size in uniform buffer properties
    for (auto& prop : m_uniforms | std::views::values) {
        if (!std::holds_alternative<ShaderUniformBufferProperty>(prop.value))
            continue;

        auto& ub = std::get<ShaderUniformBufferProperty>(prop.value);
        ub.total_size = ubo_total_size[prop.name];

        m_uniform_info[prop.name].size = ubo_total_size[prop.name];
    }
}

std::tuple<ShaderValueProperty::Type, uint32_t, uint32_t> OpenGLShaderBase::get_uniform_info(GLenum type) {
    switch (type) {
    case GL_INT:
    case GL_FLOAT:
        return {ShaderValueProperty::Type::Float, sizeof(float), sizeof(glm::vec4)};
    case GL_FLOAT_VEC2:
        return {ShaderValueProperty::Type::Vec2, sizeof(glm::vec2), sizeof(glm::vec4)};
    case GL_FLOAT_VEC3:
        return {ShaderValueProperty::Type::Vec3, sizeof(glm::vec3), sizeof(glm::vec4)};
    case GL_FLOAT_VEC4:
        return {ShaderValueProperty::Type::Vec4, sizeof(glm::vec4), sizeof(glm::vec4)};
    case GL_FLOAT_MAT3:
        return {ShaderValueProperty::Type::Mat3, sizeof(glm::mat3), sizeof(glm::mat4)};
    case GL_FLOAT_MAT4:
        return {ShaderValueProperty::Type::Mat4, sizeof(glm::mat4), sizeof(glm::mat4)};
    default:
        assert(false && "Uniform type not recognized");
    }
}

//
// OpenGLShader
//

OpenGLGraphicsShader::OpenGLGraphicsShader(const std::filesystem::path& vertex_path,
                                           const std::filesystem::path& fragment_path) {
    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_path);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_path);

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex_shader);
    glAttachShader(m_program, fragment_shader);
    glLinkProgram(m_program);

    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_program, 512, NULL, infoLog);
        MIZU_LOG_ERROR("Failed to link OpenGL Shader: {}", infoLog);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    retrieve_uniforms_info();
}

std::vector<ShaderProperty> OpenGLGraphicsShader::get_properties() const {
    return get_properties_internal();
}

std::optional<ShaderProperty> OpenGLGraphicsShader::get_property(std::string_view name) const {
    return get_property_internal(name);
}

std::optional<ShaderConstant> OpenGLGraphicsShader::get_constant(std::string_view name) const {
    return get_constant_internal(name);
}

//
// OpenGLComputeShader
//

OpenGLComputeShader::OpenGLComputeShader(const std::filesystem::path& path) {
    const GLuint compute_shader = compile_shader(GL_COMPUTE_SHADER, path);

    m_program = glCreateProgram();
    glAttachShader(m_program, compute_shader);
    glLinkProgram(m_program);

    // TODO: Check linking success?

    glDeleteShader(compute_shader);

    retrieve_uniforms_info();
}

std::vector<ShaderProperty> OpenGLComputeShader::get_properties() const {
    return get_properties_internal();
}

std::optional<ShaderProperty> OpenGLComputeShader::get_property(std::string_view name) const {
    return get_property_internal(name);
}

std::optional<ShaderConstant> OpenGLComputeShader::get_constant(std::string_view name) const {
    return get_constant_internal(name);
}

} // namespace Mizu::OpenGL
