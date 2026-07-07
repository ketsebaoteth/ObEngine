#pragma once
#include "rhi/renderer.hpp"
#include "scene/EditorCamera.hpp"
#include "scene/Scene.hpp"
#include "windowing/window.hpp"
#include <chrono>
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
  std::chrono::time_point<std::chrono::steady_clock> m_last_frame_time;
  bool isRunning;
  unsigned int frameCount;

  std::unique_ptr<IWindow> m_engine_window;
  std::unique_ptr<IRenderer> m_renderer;
  std::unique_ptr<Scene> m_active_scene;

  EditorCamera m_editor_camera;
  bool m_use_editor_camera = true;

  bool m_right_mouse_held = false;
  bool m_keys_pressed[512] = {false};
  bool m_first_mouse_move = true;
  double m_last_mouse_x = 0.0;
  double m_last_mouse_y = 0.0;
};
} // namespace ob
