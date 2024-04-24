#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <tuple>

#include "shader.h"

namespace Mizu::OpenGL {

class OpenGLShaderBase {
  public:
    virtual ~OpenGLShaderBase();

    [[nodiscard]] GLuint handle() const { return m_program; }

  protected:
    GLuint m_program;
    std::unordered_map<std::string, ShaderProperty> m_uniforms;

    [[nodiscard]] static GLuint compile_shader(GLenum type, const std::filesystem::path& path);

    [[nodiscard]] std::vector<ShaderProperty> get_properties_internal() const;
    [[nodiscard]] std::optional<ShaderProperty> get_property_internal(std::string_view name) const;

    [[nodiscard]] std::optional<ShaderConstant> get_constant_internal(std::string_view name) const;

    void retrieve_uniforms_info();

    // Returns: ShaderValueProperty::Type, type size, type padded size
    [[nodiscard]] static std::tuple<ShaderValueProperty::Type, uint32_t, uint32_t> get_uniform_info(GLenum type);
};

class OpenGLShader : public Shader, public OpenGLShaderBase {
  public:
    OpenGLShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);
    ~OpenGLShader() override = default;

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override;

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override;
};

class OpenGLComputeShader : public ComputeShader, public OpenGLShaderBase {
  public:
    OpenGLComputeShader(const std::filesystem::path& path);
    ~OpenGLComputeShader() override = default;

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override;

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override;
};

} // namespace Mizu::OpenGL