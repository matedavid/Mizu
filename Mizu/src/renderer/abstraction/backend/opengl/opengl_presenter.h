#pragma once

#include <glad/glad.h>

#include "renderer/abstraction/presenter.h"

// Forward declarations
namespace Mizu {
class Semaphore;
}

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLGraphicsPipeline;
class OpenGLImageResource;
class OpenGLBufferResource;

class OpenGLPresenter : public Presenter {
  public:
    OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~OpenGLPresenter() override;

    void present() override;
    void present(const std::shared_ptr<Semaphore>& wait_semaphore) override;

    void texture_changed(std::shared_ptr<Texture2D> texture) override;

  private:
    std::shared_ptr<Window> m_window;
    std::shared_ptr<OpenGLImageResource> m_present_texture;

    std::shared_ptr<OpenGLGraphicsPipeline> m_pipeline;
    GLint m_texture_location = -1;

    std::shared_ptr<OpenGLBufferResource> m_vertex_buffer;
    size_t m_vertex_buffer_count = 0;
};

} // namespace Mizu::OpenGL
