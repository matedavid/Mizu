#pragma once

#include "render_core/rhi/render_pass.h"

namespace Mizu::OpenGL
{

// Forward declarations
class OpenGLFramebuffer;

class OpenGLRenderPass : public RenderPass
{
  public:
    explicit OpenGLRenderPass(const Description& desc);
    ~OpenGLRenderPass() override = default;

    void begin() const;
    void end() const;

    [[nodiscard]] std::shared_ptr<Framebuffer> get_framebuffer() const override;

  private:
    std::shared_ptr<OpenGLFramebuffer> m_framebuffer;
};

} // namespace Mizu::OpenGL
