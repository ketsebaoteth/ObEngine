#include "windowing/glfw_window.hpp"
#include "GLFW/glfw3native.h"
#include "rhi/renderer.hpp"
#include "windowing/window.hpp"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <expected>

namespace ob {

GlfwWindow::GlfwWindow() {}
GlfwWindow::~GlfwWindow() { shutdown(); }
std::expected<void, std::string> GlfwWindow::init(WindowConfig &windowConfig) {
  m_config = windowConfig;
  if (!glfwInit()) {
    return std::unexpected("Failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

  m_window = glfwCreateWindow(m_config.width, m_config.height,
                              m_config.title.c_str(), nullptr, nullptr);

  if (!m_window) {
    return std::unexpected("unabled to create glfw window");
  }
  glfwShowWindow(m_window);

  uint32_t glfwExtensionsCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
  if (glfwExtensions) {
    m_extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionsCount);
  }

  glfwSetWindowUserPointer(m_window, this);

  glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int width,
                                              int height) {
    auto *self = static_cast<GlfwWindow *>(glfwGetWindowUserPointer(window));
    if (self) {
      Event event{};
      event.type = EventType::WindowResize;
      event.window.width = static_cast<uint32_t>(width);
      event.window.height = static_cast<uint32_t>(height);
      self->m_pending_events.push_back(event);
    }
  });

  glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scancode,
                                  int action, int mods) {
    auto *self = static_cast<GlfwWindow *>(glfwGetWindowUserPointer(window));
    if (self) {
      Event event{};
      event.key.code = key;
      event.key.scancode = scancode;
      event.key.mods = mods;

      if (action == GLFW_PRESS)
        event.type = EventType::KeyPressed;
      else if (action == GLFW_RELEASE)
        event.type = EventType::KeyReleased;
      else if (action == GLFW_REPEAT)
        event.type = EventType::KeyRepeat;

      self->m_pending_events.push_back(event);
    }
  });

  glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double xpos,
                                        double ypos) {
    auto *self = static_cast<GlfwWindow *>(glfwGetWindowUserPointer(window));
    if (self) {
      Event event{};
      event.type = EventType::MouseMoved;
      event.mouse_moved.x = xpos;
      event.mouse_moved.y = ypos;
      self->m_pending_events.push_back(event);
    }
  });

  glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int button,
                                          int action, int mods) {
    auto *self = static_cast<GlfwWindow *>(glfwGetWindowUserPointer(window));
    if (self) {
      Event event{};
      event.mouse_button.button = button;
      event.mouse_button.mods = mods;

      if (action == GLFW_PRESS)
        event.type = EventType::MouseButtonPressed;
      else if (action == GLFW_RELEASE)
        event.type = EventType::MouseButtonReleased;

      self->m_pending_events.push_back(event);
    }
  });

  glfwSetScrollCallback(m_window, [](GLFWwindow *window, double xoffset,
                                     double yoffset) {
    auto *self = static_cast<GlfwWindow *>(glfwGetWindowUserPointer(window));
    if (self) {
      Event event{};
      event.type = EventType::MouseScrolled;
      event.mouse_scrolled.x_offset = xoffset;
      event.mouse_scrolled.y_offset = yoffset;
      self->m_pending_events.push_back(event);
    }
  });
  return {};
}

void *GlfwWindow::get_native_window_ptr() const {
  return static_cast<void *>(m_window);
};

uint32_t GlfwWindow::get_width() const {
  int w;
  glfwGetFramebufferSize(m_window, &w, nullptr);
  return static_cast<uint32_t>(w);
}

uint32_t GlfwWindow::get_height() const {
  int h;
  glfwGetFramebufferSize(m_window, nullptr, &h);
  return static_cast<uint32_t>(h);
}
std::vector<Event> GlfwWindow::get_events() {
  glfwPollEvents();
  std::vector<Event> current_frame_events = std::move(m_pending_events);
  m_pending_events.clear();
  return current_frame_events;
}

void GlfwWindow::poll_events() { glfwPollEvents(); }
bool GlfwWindow::should_close() const {
  return glfwWindowShouldClose(m_window);
}
void GlfwWindow::shutdown() {
  if (m_window) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
  }
}

NativeWindowHandle GlfwWindow::get_native_handle() const {
  NativeWindowHandle handle{};
#if defined(_WIN32)
  handle.hwnd = static_cast<void *>(glfwGetWin32Window(m_window));

#elif defined(__linux__)
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND) && !defined(GLFW_EXPOSE_NATIVE_X11)
  handle.display = static_cast<void *>(glfwGetWaylandDisplay());
  handle.surface = static_cast<void *>(glfwGetWaylandWindow(m_window));

#elif defined(GLFW_EXPOSE_NATIVE_X11) && !defined(GLFW_EXPOSE_NATIVE_WAYLAND)
  handle.display = static_cast<void *>(glfwGetX11Display());
  handle.surface = reinterpret_cast<void *>(glfwGetX11Window(m_window));

#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND) && defined(GLFW_EXPOSE_NATIVE_X11)
  if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
    handle.display = static_cast<void *>(glfwGetWaylandDisplay());
    handle.surface = static_cast<void *>(glfwGetWaylandWindow(m_window));
  } else {
    handle.display = static_cast<void *>(glfwGetX11Display());
    handle.surface = reinterpret_cast<void *>(glfwGetX11Window(m_window));
  }
#endif
#endif
  return handle;
}

[[nodiscard]] int GlfwWindow::create_vulkan_surface(void *instance,
                                                    const void *allocator,
                                                    void *pSurface) {
  return glfwCreateWindowSurface(
      static_cast<VkInstance>(instance), m_window,
      static_cast<const VkAllocationCallbacks *>(allocator),
      static_cast<VkSurfaceKHR *>(pSurface));
};

std::vector<const char *> GlfwWindow::get_vulkan_extensions() const {
  return m_extensions;
};

} // namespace ob
