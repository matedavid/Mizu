#pragma once

#include "renderer/abstraction/presenter.h"

// Forward declarations
namespace Mizu {
class Semaphore;
}

namespace Mizu::OpenGL {

class OpenGLPresenter : public Presenter {
  public:
    OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~OpenGLPresenter() override;

    void present() override;
    void present(const std::shared_ptr<Semaphore>& wait_semaphore) override;

    void window_resized(uint32_t width, uint32_t height) override;
};

} // namespace Mizu::OpenGL
