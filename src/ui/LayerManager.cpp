#include "ui/LayerManager.hpp"
#include "rhi/renderer.hpp"
#include "scene/Scene.hpp"
#include "ui/editor/editor_layer.hpp"
#include "ui/imgui_layer.hpp"
#include <memory>

namespace ob {
LayerManager::LayerManager(IWindow *window, IRenderer *renderer,
                           EventManager *eventManager, Scene *scene) {
  auto imguiLayer =
      std::make_unique<ImGuiLayer>(window, renderer, RendererType::VULKAN);
  m_imgui_layer = imguiLayer.get(); // Save the observer pointer!
  m_layers.emplace_back(std::move(imguiLayer));
  // Inside LayerManager.cpp Constructor:
  m_layers.emplace_back(
      std::make_unique<EditorLayer>(window, renderer, eventManager, scene));
  for (auto &layer : m_layers) {
    layer->on_attach();
  }
}
LayerManager::~LayerManager() {
  for (auto &layer : m_layers) {
    layer->on_detach();
  }
}

void LayerManager::update(float deltaTime) {
  for (auto &layer : m_layers) {
    layer->on_update(deltaTime);
  }
}

void LayerManager::render() {
  if (m_imgui_layer) {
    m_imgui_layer->begin_ui_frame();
  }

  for (auto &layer : m_layers) {
    layer->on_ui_render();
  }

  ImGui::Render();
}
} // namespace ob
