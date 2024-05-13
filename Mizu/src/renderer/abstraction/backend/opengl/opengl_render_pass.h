#pragma once

#include "renderer/abstraction/render_pass.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLFramebuffer;

class OpenGLRenderPass : public RenderPass {
  public:
    explicit OpenGLRenderPass(const Description& desc);
    ~OpenGLRenderPass() override = default;

    void begin() const;
    void end() const;

  private:
    std::string m_debug_name;
    std::shared_ptr<OpenGLFramebuffer> m_framebuffer;
};

} // namespace Mizu::OpenGL
