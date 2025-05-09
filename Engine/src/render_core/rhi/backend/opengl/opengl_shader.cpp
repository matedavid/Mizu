#include "opengl_shader.h"

#include <algorithm>
#include <ranges>

#include "utility/assert.h"
#include "utility/filesystem.h"
#include "utility/logging.h"

#include "render_core/shader/shader_reflection.h"
#include "render_core/shader/shader_transpiler.h"

namespace Mizu::OpenGL
{

static void check_opengl_shader_success(GLuint shader,
                                        GLenum type,
                                        std::optional<std::filesystem::path> path_opt = std::nullopt)
{
    GLint success;
    glGetShaderiv(shader, type, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        if (path_opt.has_value())
        {
            MIZU_LOG_ERROR("OpenGL shader compilation error: {}", path_opt->string());
        }
        else
        {
            MIZU_LOG_ERROR("OpenGL shader linking error");
        }
        MIZU_LOG_ERROR("  {}", log);
    }
}

//
// OpenGLShaderBase
//

OpenGLShaderBase::~OpenGLShaderBase()
{
    glDeleteProgram(m_program);
}

GLuint OpenGLShaderBase::compile_shader(GLenum type,
                                        const std::vector<char>& source,
                                        const std::optional<std::filesystem::path>& path)
{
    const auto transpiler = ShaderTranspiler(source, ShaderTranspiler::Translation::Spirv_2_OpenGL46);
    const auto glsl_source = transpiler.compile();

    const GLuint shader = glCreateShader(type);

    const auto* c_src = glsl_source.data();
    glShaderSource(shader, 1, &c_src, nullptr);
    glCompileShader(shader);

    check_opengl_shader_success(shader, GL_COMPILE_STATUS, path);

    return shader;
}

void OpenGLShaderBase::retrieve_uniform_locations()
{
    const auto get_uniform_buffer_binding = [&](const std::string& name) -> GLint {
        const GLuint index = glGetUniformBlockIndex(m_program, name.c_str());
        MIZU_ASSERT(index != GL_INVALID_INDEX, "Could not find uniform block index for buffer with name: {}", name);

        GLint binding_point;
        glGetActiveUniformBlockiv(m_program, index, GL_UNIFORM_BLOCK_BINDING, &binding_point);
        MIZU_ASSERT(binding_point >= 0, "Could not find binding position for uniform buffer: {}", name);

        return binding_point;
    };

    for (const auto& [_, property] : m_properties)
    {
        if (std::holds_alternative<ShaderBufferProperty>(property.value))
        {
            const auto binding_point = get_uniform_buffer_binding(property.name);
            m_uniform_location.insert({property.name, binding_point});
        }
        else
        {
            const GLint location = glGetUniformLocation(m_program, property.name.c_str());
            MIZU_ASSERT(location >= 0, "Could not find uniform with name: {}", property.name);

            m_uniform_location.insert({property.name, location});
        }
    }

    for (const auto& [_, constant] : m_constants)
    {
        const auto binding_point = get_uniform_buffer_binding(constant.name);
        m_uniform_location.insert({constant.name, binding_point});
    }
}

std::vector<ShaderProperty> OpenGLShaderBase::get_properties() const
{
    std::vector<ShaderProperty> properties;
    properties.reserve(m_properties.size());

    for (const auto& [_, property] : m_properties)
    {
        properties.push_back(property);
    }

    return properties;
}

std::optional<ShaderProperty> OpenGLShaderBase::get_property(std::string_view name) const
{
    const auto it = m_properties.find(std::string(name));
    if (it == m_properties.end())
        return std::nullopt;

    return it->second;
}

std::optional<ShaderConstant> OpenGLShaderBase::get_constant(std::string_view name) const
{
    const auto it = m_constants.find(std::string(name));
    if (it == m_constants.end())
        return std::nullopt;

    return it->second;
}

std::optional<GLint> OpenGLShaderBase::get_uniform_location(std::string_view name) const
{
    const auto it = m_uniform_location.find(std::string(name));
    if (it == m_uniform_location.end())
    {
        return std::nullopt;
    }
    return it->second;
}

//
// OpenGLGraphicsShader
//

OpenGLGraphicsShader::OpenGLGraphicsShader(const ShaderStageInfo& vert_info, const ShaderStageInfo& frag_info)
{
    MIZU_ASSERT(vert_info.path.extension() == ".spv",
                "OpenGL vertex shader extension is not .spv, are you sure is a spir-v shader? At the moment OpenGL "
                "implementation only supports spir-v shaders.");
    MIZU_ASSERT(frag_info.path.extension() == ".spv",
                "OpenGL fragment shader extension is not .spv, are you sure is a spir-v shader? At the moment OpenGL "
                "implementation only supports spir-v shaders.");

    const std::vector<char> vertex_source = Filesystem::read_file(vert_info.path);
    const std::vector<char> fragment_source = Filesystem::read_file(frag_info.path);

    {
        const ShaderReflection vertex_reflection(vertex_source);

        for (const auto& property : vertex_reflection.get_properties())
        {
            m_properties.insert({property.name, property});
        }

        for (const auto& constant : vertex_reflection.get_constants())
        {
            m_constants.insert({constant.name, constant});
        }

        m_inputs = vertex_reflection.get_inputs();
    }

    {
        const ShaderReflection fragment_reflection(fragment_source);

        for (const auto& property : fragment_reflection.get_properties())
        {
            m_properties.insert({property.name, property});
        }

        for (const auto& constant : fragment_reflection.get_constants())
        {
            m_constants.insert({constant.name, constant});
        }
    }

    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source, vert_info.path);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source, frag_info.path);

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex_shader);
    glAttachShader(m_program, fragment_shader);
    glLinkProgram(m_program);

    check_opengl_shader_success(m_program, GL_LINK_STATUS);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    retrieve_uniform_locations();
}

std::vector<ShaderOutput> OpenGLGraphicsShader::get_outputs() const
{
    MIZU_UNREACHABLE("Unimplemented");
    return {};
}

//
// OpenGLComputeShader
//

OpenGLComputeShader::OpenGLComputeShader(const ShaderStageInfo& comp_info)
{
    const std::vector<char> source = Filesystem::read_file(comp_info.path);

    {
        const ShaderReflection compute_reflection(source);

        for (const auto& property : compute_reflection.get_properties())
        {
            m_properties.insert({property.name, property});
        }

        for (const auto& constant : compute_reflection.get_constants())
        {
            m_constants.insert({constant.name, constant});
        }
    }

    const GLuint compute_shader = compile_shader(GL_COMPUTE_SHADER, source, comp_info.path);

    m_program = glCreateProgram();
    glAttachShader(m_program, compute_shader);
    glLinkProgram(m_program);

    check_opengl_shader_success(m_program, GL_LINK_STATUS);

    glDeleteShader(compute_shader);

    retrieve_uniform_locations();
}

} // namespace Mizu::OpenGL
