#pragma once

#include "input/keycodes.h"

#include <glm/glm.hpp>
#include <utility>

namespace Mizu
{

enum class EventType
{
    None,
    // Window Events
    WindowResized,
    // Mouse Events
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseScrolled,
    // Keyboard Events
    KeyPressed,
    KeyReleased,
    KeyRepeated,
};

class Event
{
  public:
    virtual ~Event() = default;

    bool handled = false;
    [[nodiscard]] virtual EventType get_type() const = 0;
};

class WindowResizedEvent : public Event
{
  public:
    WindowResizedEvent(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

    [[nodiscard]] EventType get_type() const override { return EventType::WindowResized; }

    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] std::pair<uint32_t, uint32_t> get_dimensions() const { return {m_width, m_height}; }

  private:
    uint32_t m_width, m_height;
};

class MouseMovedEvent : public Event
{
  public:
    MouseMovedEvent(double xpos, double ypos, glm::vec2 movement) : m_xpos(xpos), m_ypos(ypos), m_movement(movement) {}

    [[nodiscard]] EventType get_type() const override { return EventType::MouseMoved; }

    [[nodiscard]] double get_xpos() const { return m_xpos; }
    [[nodiscard]] double get_ypos() const { return m_ypos; }
    [[nodiscard]] glm::vec2 get_movement() const { return m_movement; }

  private:
    double m_xpos, m_ypos;
    glm::vec2 m_movement;
};

class MousePressedEvent : public Event
{
  public:
    MousePressedEvent(MouseButton button, ModifierKeyBits mods) : m_button(button), m_mods(mods) {}

    [[nodiscard]] EventType get_type() const override { return EventType::MouseButtonPressed; }

    [[nodiscard]] MouseButton get_button() const { return m_button; }
    [[nodiscard]] ModifierKeyBits get_mods() const { return m_mods; }

  private:
    MouseButton m_button;
    ModifierKeyBits m_mods;
};

class MouseReleasedEvent : public Event
{
  public:
    MouseReleasedEvent(MouseButton button, ModifierKeyBits mods) : m_button(button), m_mods(mods) {}

    [[nodiscard]] EventType get_type() const override { return EventType::MouseButtonReleased; }

    [[nodiscard]] MouseButton get_button() const { return m_button; }
    [[nodiscard]] ModifierKeyBits get_mods() const { return m_mods; }

  private:
    MouseButton m_button;
    ModifierKeyBits m_mods;
};

class MouseScrolledEvent : public Event
{
  public:
    MouseScrolledEvent(double xoffset, double yoffset) : m_xoffset(xoffset), m_yoffset(yoffset) {}

    [[nodiscard]] EventType get_type() const override { return EventType::MouseScrolled; }

    [[nodiscard]] double get_xoffset() const { return m_xoffset; }
    [[nodiscard]] double get_yoffset() const { return m_yoffset; }

    [[nodiscard]] std::pair<double, double> get_offset() const { return {m_xoffset, m_yoffset}; }

  private:
    double m_xoffset, m_yoffset;
};

class KeyPressedEvent : public Event
{
  public:
    KeyPressedEvent(Key key, int32_t scancode, ModifierKeyBits mods) : m_key(key), m_scancode(scancode), m_mods(mods) {}

    [[nodiscard]] EventType get_type() const override { return EventType::KeyPressed; }

    [[nodiscard]] Key get_key() const { return m_key; }
    [[nodiscard]] int32_t get_scancode() const { return m_scancode; }
    [[nodiscard]] ModifierKeyBits get_mods() const { return m_mods; }

  private:
    Key m_key;
    int32_t m_scancode;
    ModifierKeyBits m_mods;
};

class KeyReleasedEvent : public Event
{
  public:
    KeyReleasedEvent(Key key, int32_t scancode, ModifierKeyBits mods) : m_key(key), m_scancode(scancode), m_mods(mods)
    {
    }

    [[nodiscard]] EventType get_type() const override { return EventType::KeyReleased; }

    [[nodiscard]] Key get_key() const { return m_key; }
    [[nodiscard]] int32_t get_scancode() const { return m_scancode; }
    [[nodiscard]] ModifierKeyBits get_mods() const { return m_mods; }

  private:
    Key m_key;
    int32_t m_scancode;
    ModifierKeyBits m_mods;
};

class KeyRepeatEvent : public Event
{
  public:
    KeyRepeatEvent(Key key, int32_t scancode, ModifierKeyBits mods) : m_key(key), m_scancode(scancode), m_mods(mods) {}

    [[nodiscard]] EventType get_type() const override { return EventType::KeyRepeated; }

    [[nodiscard]] Key get_key() const { return m_key; }
    [[nodiscard]] int32_t get_scancode() const { return m_scancode; }
    [[nodiscard]] ModifierKeyBits get_mods() const { return m_mods; }

  private:
    Key m_key;
    int32_t m_scancode;
    ModifierKeyBits m_mods;
};

} // namespace Mizu