#pragma once

#include "render_pass.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLFramebuffer;

class OpenGLRenderPass : public RenderPass {
  public:
    OpenGLRenderPass(const Description& desc);
    ~OpenGLRenderPass() override = default;

    void begin(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    void end(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;

  private:
    std::string m_debug_name;
    std::shared_ptr<OpenGLFramebuffer> m_framebuffer;
};

} // namespace Mizu::OpenGL
