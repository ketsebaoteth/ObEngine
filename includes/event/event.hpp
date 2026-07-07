#pragma once

#include <cstdint>

namespace ob {

enum class EventType : uint32_t {
  None = 0,
  WindowClose,
  WindowResize,
  KeyPressed,
  KeyReleased,
  KeyRepeat,
  MouseButtonPressed,
  MouseButtonReleased,
  MouseMoved,
  MouseScrolled
};

struct WindowResizeEvent {
  uint32_t width;
  uint32_t height;
};

struct KeyEvent {
  int32_t code;
  int32_t scancode;
  int32_t mods;
};

struct MouseButtonEvent {
  int32_t button;
  int32_t mods;
};

struct MouseMovedEvent {
  double x;
  double y;
};

struct MouseScrolledEvent {
  double x_offset;
  double y_offset;
};

struct Event {
  EventType type = EventType::None;
  bool handled = false;

  union {
    WindowResizeEvent window;
    KeyEvent key;
    MouseButtonEvent mouse_button;
    MouseMovedEvent mouse_moved;
    MouseScrolledEvent mouse_scrolled;
  };
};

} // namespace ob
