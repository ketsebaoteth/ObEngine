#pragma once

#include "event/event.hpp"
#include "rhi/renderer.hpp"
#include <cstdint>
#include <expected>
#include <string>

namespace ob {
struct WindowConfig {
  std::string title = "Oblique Engine";
  uint32_t width = 1280;
  uint32_t height = 720;
};

class IWindow {
public:
  virtual ~IWindow() = default;

  virtual std::expected<void, std::string> init(WindowConfig &WindowConfig) = 0;
  virtual void poll_events() = 0;
  [[nodiscard]] virtual bool should_close() const = 0;
  virtual void shutdown() = 0;
  [[nodiscard]] virtual uint32_t get_width() const = 0;
  [[nodiscard]] virtual uint32_t get_height() const = 0;

  virtual NativeWindowHandle get_native_handle() const = 0;
  virtual std::vector<Event> get_events() = 0;
};

} // namespace ob
