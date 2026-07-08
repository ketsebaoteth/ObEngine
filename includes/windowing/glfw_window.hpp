#pragma once

#include "event/engine_events.hpp"
#include "rhi/renderer.hpp"
#include "windowing/window.hpp"
#include <expected>
#include <string>
#include <vector>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace ob {

class GlfwWindow : public IWindow {
public:
  GlfwWindow();
  ~GlfwWindow() override;

  std::expected<void, std::string> init(WindowConfig &windowConfig) override;
  void poll_events() override;
  [[nodiscard]] bool should_close() const override;
  void shutdown() override;
  std::vector<Event> get_events() override;

  virtual NativeWindowHandle get_native_handle() const override;
  void *get_native_window_ptr() const override;

  [[nodiscard]] uint32_t get_width() const override;
  [[nodiscard]] uint32_t get_height() const override;

  [[nodiscard]] std::vector<const char *>
  get_vulkan_extensions() const override;
  [[nodiscard]] int create_vulkan_surface(void *instance, const void *allocator,
                                          void *pSurface) override;

private:
  WindowConfig m_config;
  GLFWwindow *m_window = nullptr;
  std::vector<const char *> m_extensions;
  std::vector<Event> m_pending_events;
};

} // namespace ob
