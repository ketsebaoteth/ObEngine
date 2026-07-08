#pragma once
#include "engine/layer.hpp"
#include "imgui.h"
#include "rhi/renderer.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "windowing/window.hpp"
#include <memory>

namespace ob {
class IWindow;
class ImGuiLayer : public Layer {
public:
  ImGuiLayer(IWindow *window, IRenderer *renderer, RendererType type);
  ~ImGuiLayer() override = default;

  void on_attach() override;
  void on_detach() override;
  void on_ui_render() override;
  void begin_ui_frame();

  [[nodiscard]] VkDescriptorSet getViewportDescriptorSet() const {
    return m_viewportDescriptorSet;
  }

private:
  IWindow *m_window;
  IRenderer *m_renderer;
  RendererType m_renderer_type;

  VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
  VkDescriptorSet m_viewportDescriptorSet = VK_NULL_HANDLE;
  VkImageView m_lastTrackedImageView = VK_NULL_HANDLE;

  VkImageView m_viewportImageView = VK_NULL_HANDLE;
  VkSampler m_viewportSampler = VK_NULL_HANDLE;
};

} // namespace ob
