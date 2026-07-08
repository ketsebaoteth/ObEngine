// In editor_layer.cpp
#include "ui/editor/editor_layer.hpp"
#include "event/event_manager.hpp"
#include "windowing/window.hpp"
#include <GLFW/glfw3.h> // Or your engine event key codes

namespace ob {

EditorLayer::EditorLayer(IWindow *window, IRenderer *renderer,
                         EventManager *eventManager, Scene *scene)
    : m_window(window), m_renderer(renderer), m_eventManager(eventManager),
      m_scene(scene) {}

void EditorLayer::on_attach() {
  m_camera_entity = m_scene->createEntity();

  m_scene->registry().emplace<CameraComponent>(m_camera_entity);
  m_scene->registry().emplace<TransformComponent>(
      m_camera_entity, glm::vec3(0.0f, 0.0f, 5.0f), // Start camera 5 units back
      glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));

  m_scene->setActiveRuntimeCamera(m_camera_entity);

  setupInputBindings();
}

void EditorLayer::on_detach() {
  if (m_camera_entity != entt::null) {
    m_scene->registry().destroy(m_camera_entity);
    m_camera_entity = entt::null;
  }
}

// Inside editor_layer.cpp
void EditorLayer::setupInputBindings() {
  m_eventManager->registerActionCallback(
      "MoveForwardStart", [this](ActionDetails) { m_moving_forward = true; });
  m_eventManager->registerActionCallback(
      "MoveForwardStop", [this](ActionDetails) { m_moving_forward = false; });
  m_eventManager->registerActionCallback(
      "MoveBackwardStart", [this](ActionDetails) { m_moving_backward = true; });
  m_eventManager->registerActionCallback(
      "MoveBackwardStop", [this](ActionDetails) { m_moving_backward = false; });
  m_eventManager->registerActionCallback(
      "MoveLeftStart", [this](ActionDetails) { m_moving_left = true; });
  m_eventManager->registerActionCallback(
      "MoveLeftStop", [this](ActionDetails) { m_moving_left = false; });
  m_eventManager->registerActionCallback(
      "MoveRightStart", [this](ActionDetails) { m_moving_right = true; });
  m_eventManager->registerActionCallback(
      "MoveRightStop", [this](ActionDetails) { m_moving_right = false; });

  m_eventManager->registerActionCallback("LookAroundStart",
                                         [this](ActionDetails) {
                                           m_right_mouse_held = true;
                                           m_first_mouse_move = true;
                                         });

  m_eventManager->registerActionCallback(
      "LookAroundStop", [this](ActionDetails) { m_right_mouse_held = false; });

  m_eventManager->registerActionCallback(
      "LookAroundMove", [this](ActionDetails details) {
        if (m_right_mouse_held) {
          if (m_first_mouse_move) {
            m_last_mouse_x = details.mouseLandPos.x;
            m_last_mouse_y = details.mouseLandPos.y;
            m_first_mouse_move = false;
          }

          float sensitivityX = 0.06f;
          float sensitivityY = 0.06f;

          double delta_x =
              (m_last_mouse_x - details.mouseLandPos.x) * sensitivityX;
          double delta_y = (m_last_mouse_y - details.mouseLandPos.y) *
                           sensitivityY; // Swapped subtraction to invert Y!

          m_last_mouse_x = details.mouseLandPos.x;
          m_last_mouse_y = details.mouseLandPos.y;

          m_editor_camera.processMouseMovement(static_cast<float>(delta_x),
                                               static_cast<float>(delta_y));
        }
      });
}

void EditorLayer::on_update(float deltaTime) {
  float cameraSpeed = 5.0f * deltaTime;

  if (m_moving_forward)
    m_editor_camera.moveForward(cameraSpeed);
  if (m_moving_backward)
    m_editor_camera.moveForward(-cameraSpeed);
  if (m_moving_left)
    m_editor_camera.moveRight(-cameraSpeed);
  if (m_moving_right)
    m_editor_camera.moveRight(cameraSpeed);

  if (m_camera_entity != entt::null) {
    auto &transform =
        m_scene->registry().get<TransformComponent>(m_camera_entity);
    transform.translation = m_editor_camera.position;
    transform.rotation.x = glm::radians(m_editor_camera.rotation.x);
    transform.rotation.y = glm::radians(m_editor_camera.rotation.y + 90.0f);
    transform.rotation.z = glm::radians(m_editor_camera.rotation.z);
  }
}

void EditorLayer::on_ui_render() {}

} // namespace ob
