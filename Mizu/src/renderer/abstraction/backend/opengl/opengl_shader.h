#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <tuple>
#include <unordered_map>

#include "renderer/abstraction/shader.h"

namespace Mizu::OpenGL {

class OpenGLShaderBase : public virtual IShader {
  public:
    virtual ~OpenGLShaderBase();

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const;
    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const;

    [[nodiscard]] std::optional<GLint> get_uniform_location(std::string_view name) const;

    [[nodiscard]] GLuint handle() const { return m_program; }

  protected:
    GLuint m_program;

    std::unordered_map<std::string, ShaderProperty> m_properties;
    std::unordered_map<std::string, ShaderConstant> m_constants;

    std::unordered_map<std::string, GLint> m_uniform_location;
    std::unordered_map<std::string, GLuint> m_uniform_buffer_binding_point;

    [[nodiscard]] GLuint compile_shader(GLenum type, const std::filesystem::path& path);
};

class OpenGLGraphicsShader : public GraphicsShader, public OpenGLShaderBase {
  public:
    OpenGLGraphicsShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);
};

class OpenGLComputeShader : public ComputeShader, public OpenGLShaderBase {
  public:
    OpenGLComputeShader(const std::filesystem::path& path);
};

} // namespace Mizu::OpenGL