#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <tuple>
#include <unordered_map>

#include "renderer/abstraction/shader.h"

namespace Mizu::OpenGL {

enum class OpenGLUniformType {
    Texture,
    UniformBuffer,
};

struct OpenGLUniformInfo {
    OpenGLUniformType type;
    uint32_t binding;
    uint32_t size;
    GLint location;
};

class OpenGLShaderBase {
  public:
    virtual ~OpenGLShaderBase();

    [[nodiscard]] std::optional<OpenGLUniformInfo> get_uniform_info(std::string_view name) const;

    [[nodiscard]] GLuint handle() const { return m_program; }

  protected:
    GLuint m_program;
    std::unordered_map<std::string, ShaderProperty> m_uniforms;
    std::unordered_map<std::string, OpenGLUniformInfo> m_uniform_info;

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
    explicit OpenGLComputeShader(const std::filesystem::path& path);
    ~OpenGLComputeShader() override = default;

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override;
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override;

    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override;
};

} // namespace Mizu::OpenGL