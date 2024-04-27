#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>

#include "graphics_pipeline.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLShader;
class OpenGLUniformBuffer;
struct OpenGLUniformInfo;
enum class OpenGLUniformType;

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

    struct TextureUniformInfo {
        uint32_t texture_handle;
        uint32_t binding;
    };

    struct UniformBufferUniformInfo {
        uint32_t ubo_handle;
        uint32_t binding;
        uint32_t size;
    };

    using UniformInfoT = std::variant<TextureUniformInfo, UniformBufferUniformInfo>;
    std::unordered_map<std::string, std::optional<UniformInfoT>> m_uniform_info;

    std::unordered_map<std::string, std::shared_ptr<OpenGLUniformBuffer>> m_constants;

    [[nodiscard]] std::optional<OpenGLUniformInfo> get_uniform_info(std::string_view name,
                                                                    OpenGLUniformType type,
                                                                    std::string_view type_name) const;
};

} // namespace Mizu::OpenGL
