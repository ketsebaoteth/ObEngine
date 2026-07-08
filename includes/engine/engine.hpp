// In engine.hpp
#pragma once
#include "event/event_manager.hpp"
#include "rhi/renderer.hpp"
#include "scene/Scene.hpp"
#include "ui/LayerManager.hpp"
#include "windowing/window.hpp"
#include <chrono>
#include <expected>
#include <memory>

namespace ob {

class Engine {
public:
  Engine();
  ~Engine();
  std::expected<void, std::string> init();
  void run();

private:
  void update(float deltaTime);
  void render();

private:
  RendererType m_active_renderer_type;
  std::chrono::time_point<std::chrono::steady_clock> m_last_frame_time;
  bool isRunning;
  unsigned int frameCount;

  // Managed Core Systems
  std::unique_ptr<WindowManager> m_window_manager;
  std::unique_ptr<EventManager> m_event_manager;
  std::unique_ptr<LayerManager> m_layer_manager;

  std::unique_ptr<IRenderer> m_renderer;
  std::unique_ptr<Scene> m_active_scene;
};

} // namespace ob
