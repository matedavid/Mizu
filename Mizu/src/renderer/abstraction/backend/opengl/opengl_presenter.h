#pragma once

#include "renderer/abstraction/presenter.h"

// Forward declarations
namespace Mizu {
class Semaphore;
}

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLGraphicsShader;
class OpenGLTexture2D;
class OpenGLVertexBuffer;

class OpenGLPresenter : public Presenter {
  public:
    OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~OpenGLPresenter() override;

    void present() override;
    void present(const std::shared_ptr<Semaphore>& wait_semaphore) override;

    void texture_changed(std::shared_ptr<Texture2D> texture) override;

  private:
    std::shared_ptr<Window> m_window;
    std::shared_ptr<OpenGLTexture2D> m_present_texture;

    std::shared_ptr<OpenGLGraphicsShader> m_present_shader;
    GLint m_texture_location = -1;

    std::shared_ptr<OpenGLVertexBuffer> m_vertex_buffer;
};

} // namespace Mizu::OpenGL
