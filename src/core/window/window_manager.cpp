#include "rhi/renderer.hpp"
#include "windowing/glfw_window.hpp"
#include "windowing/window.hpp"
#include <X11/X.h>
#include <memory>

namespace ob {
WindowManager::WindowManager() {
  WindowConfig config{};
  config.width = 1200;
  config.height = 920;
  config.title = "ob Engine";
  m_config = config;
}

WindowManager::~WindowManager() { m_window_impl->shutdown(); }

std::expected<void, std::string> WindowManager::init() {
  m_window_impl = std::make_unique<GlfwWindow>();
  return m_window_impl->init(m_config);
}

RendererConfig WindowManager::get_renderer_config(RendererType type) {
  RendererConfig config{};
  switch (type) {
  case ob::RendererType::VULKAN:
    config.width = m_config.width;
    config.height = m_config.height;
    config.vulkanExtensions = m_window_impl->get_vulkan_extensions();
#ifndef NDEBUG
    config.validation = true;
#else
    config.validation = false;
#endif
    config.surface_creator = [&](void *instance, const void *alloc,
                                 void *pSurface) {
      return m_window_impl->create_vulkan_surface(instance, alloc, pSurface);
    };
    config.vsync = true;
    return config;
  default:
    return {};
  }
}
} // namespace ob
