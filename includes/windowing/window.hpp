#pragma once

#include "event/event.hpp"
#include "rhi/renderer.hpp"
#include "rhi/vulkan_renderer.hpp"
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

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
  virtual void *get_native_window_ptr() const = 0;

  [[nodiscard]] virtual std::vector<const char *>
  get_vulkan_extensions() const {
    return {};
  }
  [[nodiscard]] virtual int create_vulkan_surface(void *instance,
                                                  const void *allocator,
                                                  void *pSurface) = 0;

  virtual std::vector<Event> get_events() = 0;
};

class WindowManager {
public:
  WindowManager();
  ~WindowManager();

  std::expected<void, std::string> init();
  RendererConfig get_renderer_config(RendererType type);

  IWindow *get_window_impl() { return m_window_impl.get(); }

private:
  WindowConfig m_config;
  std::unique_ptr<IWindow> m_window_impl;
};

} // namespace ob
