#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <tuple>
#include <unordered_map>

#include "renderer/abstraction/shader.h"

namespace Mizu::OpenGL {

class OpenGLShaderBase {
  public:
    virtual ~OpenGLShaderBase();

    [[nodiscard]] std::vector<ShaderProperty> get_properties_base() const;
    [[nodiscard]] std::optional<ShaderProperty> get_property_base(std::string_view name) const;
    [[nodiscard]] std::optional<ShaderConstant> get_constant_base(std::string_view name) const;

    [[nodiscard]] GLuint handle() const { return m_program; }

  protected:
    GLuint m_program;

    std::unordered_map<std::string, ShaderProperty> m_properties;
    std::unordered_map<std::string, ShaderConstant> m_constants;

    [[nodiscard]] GLuint compile_shader(GLenum type, const std::filesystem::path& path);
};

class OpenGLGraphicsShader : public GraphicsShader, public OpenGLShaderBase {
  public:
    OpenGLGraphicsShader(const std::filesystem::path& vertex_path, const std::filesystem::path& fragment_path);

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override { return get_properties_base(); }
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override {
        return get_property_base(name);
    }
    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override {
        return get_constant_base(name);
    }
};

class OpenGLComputeShader : public ComputeShader, public OpenGLShaderBase {
  public:
    OpenGLComputeShader(const std::filesystem::path& path);

    [[nodiscard]] std::vector<ShaderProperty> get_properties() const override { return get_properties_base(); }
    [[nodiscard]] std::optional<ShaderProperty> get_property(std::string_view name) const override {
        return get_property_base(name);
    }
    [[nodiscard]] std::optional<ShaderConstant> get_constant(std::string_view name) const override {
        return get_constant_base(name);
    }
};

} // namespace Mizu::OpenGL