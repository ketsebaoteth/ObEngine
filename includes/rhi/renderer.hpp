#pragma once
#include <cstdint>
// #include <span>
#include "scene/Components.hpp"
#include <entt/entt.hpp>
#include <expected>
#include <string>
#include <vector>

namespace ob {

struct RenderItem {
  MeshHandle handle{0};
  glm::mat4 transform;
};

struct NativeWindowHandle {
  void *hwnd = nullptr;    // Windows HWND
  void *display = nullptr; // X11 Display*
  void *surface = nullptr; // Wayland wl_surface*
  void *window = nullptr;  // GLFWwindow* or ANativeWindow*
  std::vector<const char *> required_instance_extensions;
};

struct RendererConfig {
  uint32_t width;
  uint32_t height;
  bool vsync = true;
  bool validation = false;
};

class IRenderer {
public:
  virtual ~IRenderer() = default;

  virtual MeshHandle uploadMesh(std::span<const Vertex> vertices,
                                std::span<const uint32_t> indices) = 0;
  virtual void destroyMesh(MeshHandle handle) = 0;

  virtual std::expected<void, std::string> init(const NativeWindowHandle &,
                                                const RendererConfig &) = 0;
  virtual void waitDeviceIdle() = 0;
  virtual void shutdown() = 0;
  virtual void resize(uint32_t w, uint32_t h) = 0;
  virtual void present(std::span<const RenderItem> renderQueue,
                       const glm::mat4 &view, const glm::mat4 &proj) = 0;
};
} // namespace ob
