#pragma once

#include <cstdint>
#include <glad/glad.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

#include "renderer/abstraction/graphics_pipeline.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLGraphicsShader;
class OpenGLUniformBuffer;

class OpenGLGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit OpenGLGraphicsPipeline(const Description& desc);
    ~OpenGLGraphicsPipeline() override = default;

    void set_state() const;

    void push_constant(std::string_view name, uint32_t size, const void* data);

    [[nodiscard]] std::shared_ptr<OpenGLGraphicsShader> get_shader() const { return m_shader; }

  private:
    std::shared_ptr<OpenGLGraphicsShader> m_shader;
    Description m_description;

    std::unordered_map<std::string, std::shared_ptr<OpenGLUniformBuffer>> m_constants;

    [[nodiscard]] static GLenum get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static GLenum get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static GLenum get_front_face(RasterizationState::FrontFace face);
    [[nodiscard]] static GLenum get_depth_bias_polygon_mode(RasterizationState::PolygonMode mode);

    [[nodiscard]] static GLenum get_depth_func(DepthStencilState::DepthCompareOp op);
};

} // namespace Mizu::OpenGL
