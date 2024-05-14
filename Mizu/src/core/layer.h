#pragma once

#include "input/events.h"

namespace Mizu {

class Layer {
  public:
    virtual ~Layer() = default;

    virtual void on_update(double ts) = 0;

    virtual void on_window_resized([[maybe_unused]] WindowResizeEvent&) {}
    virtual void on_mouse_moved([[maybe_unused]] MouseMovedEvent&) {}
    virtual void on_mouse_pressed([[maybe_unused]] MousePressedEvent&) {}
    virtual void on_mouse_released([[maybe_unused]] MouseReleasedEvent&) {}
    virtual void on_mouse_scrolled([[maybe_unused]] MouseScrolledEvent&) {}
    virtual void on_key_pressed([[maybe_unused]] KeyPressedEvent&) {}
    virtual void on_key_released([[maybe_unused]] KeyReleasedEvent&) {}
    virtual void on_key_repeat([[maybe_unused]] KeyRepeatEvent&) {}
};

} // namespace Mizu