#pragma once

#include "core/events.h"

namespace Mizu
{

class GameSimulation
{
  public:
    virtual ~GameSimulation() = default;

    virtual void init() = 0;
    virtual void update(double dt) = 0;

    virtual void on_window_resized(WindowResizedEvent& /*event*/) {}
    virtual void on_mouse_moved(MouseMovedEvent& /*event*/) {}
    virtual void on_mouse_pressed(MousePressedEvent& /*event*/) {}
    virtual void on_mouse_released(MouseReleasedEvent& /*event*/) {}
    virtual void on_mouse_scrolled(MouseScrolledEvent& /*event*/) {}
    virtual void on_key_pressed(KeyPressedEvent& /*event*/) {}
    virtual void on_key_released(KeyReleasedEvent& /*event*/) {}
    virtual void on_key_repeat(KeyRepeatEvent& /*event*/) {}
};

} // namespace Mizu