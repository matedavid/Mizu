#pragma once

#include <optional>
#include <unordered_map>
#include <variant>

#include "graphics_pipeline.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLShader;
class OpenGLTexture2D;
class OpenGLUniformBuffer;

class OpenGLGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit OpenGLGraphicsPipeline(const Description& desc);
    ~OpenGLGraphicsPipeline() override = default;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    [[nodiscard]] bool bake() override;

    void add_input(std::string_view name, const std::shared_ptr<Texture2D>& texture) override;
    void add_input(std::string_view name, const std::shared_ptr<UniformBuffer>& ub) override;

    [[nodiscard]] bool push_constant(const std::shared_ptr<ICommandBuffer>& command_buffer,
                                     std::string_view name,
                                     uint32_t size,
                                     const void* data) override;

  private:
    std::shared_ptr<OpenGLShader> m_shader;
    Description m_description;

    using UniformInfoT = std::variant<std::shared_ptr<OpenGLTexture2D>, std::shared_ptr<OpenGLUniformBuffer>>;
    std::unordered_map<std::string, std::optional<UniformInfoT>> m_uniforms;
};

} // namespace Mizu::OpenGL
