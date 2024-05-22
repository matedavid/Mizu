#include "opengl_presenter.h"

namespace Mizu::OpenGL {

OpenGLPresenter::OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture) {}

OpenGLPresenter::~OpenGLPresenter() {}

void OpenGLPresenter::present() {
    present(nullptr);
}

void OpenGLPresenter::present(const std::shared_ptr<Semaphore>& wait_semaphore) {}

void OpenGLPresenter::window_resized(uint32_t width, uint32_t height) {}

} // namespace Mizu::OpenGL