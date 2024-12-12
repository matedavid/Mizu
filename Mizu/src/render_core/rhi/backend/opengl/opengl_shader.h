#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <tuple>
#include <unordered_map>

#include "render_core/rhi/shader.h"

namespace Mizu::OpenGL
{

class OpenGLShaderBase : public virtual IShader
{
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

    [[nodiscard]] GLuint compile_shader(GLenum type,
                                        const std::vector<char>& source,
                                        const std::optional<std::filesystem::path>& path);
    void retrieve_uniform_locations();
};

class OpenGLGraphicsShader : public GraphicsShader, public OpenGLShaderBase
{
  public:
    OpenGLGraphicsShader(const ShaderStageInfo& vert_info, const ShaderStageInfo& frag_info);

    [[nodiscard]] std::vector<ShaderInput> get_inputs() const { return m_inputs; }

  private:
    std::vector<ShaderInput> m_inputs;
};

class OpenGLComputeShader : public ComputeShader, public OpenGLShaderBase
{
  public:
    OpenGLComputeShader(const ShaderStageInfo& comp_info);
};

} // namespace Mizu::OpenGL