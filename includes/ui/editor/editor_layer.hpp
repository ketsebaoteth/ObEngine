// In editor_layer.hpp
#pragma once
#include "engine/layer.hpp"
#include "scene/EditorCamera.hpp"
#include "scene/Scene.hpp"
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

namespace ob {

class IWindow;
class IRenderer;
class EventManager;

class EditorLayer : public Layer {
public:
  EditorLayer(IWindow *window, IRenderer *renderer, EventManager *eventManager,
              Scene *scene);
  ~EditorLayer() override = default;

  void on_attach() override;
  void on_detach() override;
  void on_update(float deltaTime) override;
  void on_ui_render() override;

private:
  void setupInputBindings();
  void drawHierarchyPanel();
  void drawInspectorPanel();

private:
  IWindow *m_window;
  IRenderer *m_renderer;
  EventManager *m_eventManager;
  Scene *m_scene;

  // The actual camera controller and state
  EditorCamera m_editor_camera;
  entt::entity m_camera_entity = entt::null;
  entt::entity m_selected_entity = entt::null;
  bool m_popup_opened = false;
  // Input states
  bool m_right_mouse_held = false;
  bool m_keys_pressed[512] = {false};
  bool m_first_mouse_move = true;
  double m_last_mouse_x = 0.0;
  double m_last_mouse_y = 0.0;

  bool m_moving_forward = false;
  bool m_moving_backward = false;
  bool m_moving_left = false;
  bool m_moving_right = false;
};

} // namespace ob
