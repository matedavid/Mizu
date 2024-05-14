#pragma once

#include "renderer/abstraction/presenter.h"

namespace Mizu::OpenGL {

class OpenGLPresenter : public Presenter {
  public:
    OpenGLPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~OpenGLPresenter() override;

    void present() override;
    void window_resized(uint32_t width, uint32_t height) override;
};

} // namespace Mizu::OpenGL
