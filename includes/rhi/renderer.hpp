#pragma once
#include <cstdint>
// #include <span>
#include "scene/Components.hpp"
#include <entt/entt.hpp>
#include <expected>
#include <string>

namespace ob {
class VulkanContext;

struct RenderItem {
  MeshHandle handle{0};
  glm::mat4 transform;
};

struct NativeWindowHandle {
#if defined(_WIN32)
  void *hwnd = nullptr;
#elif defined(__linux__)
  void *display = nullptr;
  void *surface = nullptr;
#endif
};

struct RendererConfig {
  uint32_t width;
  uint32_t height;
  bool vsync = true;

  // vulkan related
  std::vector<const char *> vulkanExtensions;
  bool validation = false;
  std::function<int(void *, const void *, void *)> surface_creator;
};

enum class RendererType { VULKAN, OPENGL, D3D };

class IRenderer {
public:
  virtual ~IRenderer() = default;

  virtual MeshHandle uploadMesh(std::span<const Vertex> vertices,
                                std::span<const uint32_t> indices) = 0;
  virtual void destroyMesh(MeshHandle handle) = 0;

  virtual std::expected<void, std::string> init(const RendererConfig &) = 0;
  virtual void waitDeviceIdle() = 0;
  virtual void shutdown() = 0;
  virtual void resize(uint32_t w, uint32_t h) = 0;
  virtual void present(std::span<const RenderItem> renderQueue,
                       const glm::mat4 &view, const glm::mat4 &proj) = 0;

  virtual void *get_viewport_texture_id() const = 0;

  [[nodiscard]] virtual VulkanContext get_vulkan_context() const = 0;
  virtual VkImageView getViewportImageView() const = 0;
  virtual void register_imgui_viewport_texture() = 0;
  virtual VkSampler getViewportSampler() const = 0;
};
} // namespace ob
