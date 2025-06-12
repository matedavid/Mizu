#pragma once

#include <cstdint>
#include <glad/glad.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/shader/shader_properties.h"

namespace Mizu::OpenGL
{

/*

// Forward declarations
class OpenGLGraphicsShader;
class OpenGLBufferResource;

class OpenGLGraphicsPipeline : public GraphicsPipeline
{
  public:
    explicit OpenGLGraphicsPipeline(const Description& desc);
    ~OpenGLGraphicsPipeline() override;

    void set_state() const;

    void push_constant(std::string_view name, uint32_t size, const void* data);

    void set_vertex_buffer_layout();

    [[nodiscard]] std::shared_ptr<OpenGLGraphicsShader> get_shader() const { return m_shader; }

  private:
    std::shared_ptr<OpenGLGraphicsShader> m_shader;
    GLuint m_vao{};
    Description m_description;

    std::unordered_map<std::string, std::shared_ptr<OpenGLBufferResource>> m_constants;

    [[nodiscard]] static GLenum get_polygon_mode(RasterizationState::PolygonMode mode);
    [[nodiscard]] static GLenum get_cull_mode(RasterizationState::CullMode mode);
    [[nodiscard]] static GLenum get_front_face(RasterizationState::FrontFace face);
    [[nodiscard]] static GLenum get_depth_bias_polygon_mode(RasterizationState::PolygonMode mode);

    [[nodiscard]] static GLenum get_depth_func(DepthStencilState::DepthCompareOp op);

    [[nodiscard]] static GLenum get_opengl_type(ShaderValueType type);
    [[nodiscard]] static uint32_t get_type_count(ShaderValueType type);
};

*/

} // namespace Mizu::OpenGL
