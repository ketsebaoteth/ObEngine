// In editor_layer.cpp
#include "ui/editor/editor_layer.hpp"
#include "event/event_manager.hpp"
#include "imgui.h"
#include "log/log.hpp"
#include "windowing/window.hpp"
#include <GLFW/glfw3.h>

namespace ob {

EditorLayer::EditorLayer(IWindow *window, IRenderer *renderer,
                         EventManager *eventManager, Scene *scene)
    : m_window(window), m_renderer(renderer), m_eventManager(eventManager),
      m_scene(scene) {}

void EditorLayer::on_attach() {
  m_camera_entity = m_scene->createEntity();

  m_scene->registry().emplace<CameraComponent>(m_camera_entity);

  // Match baseline camera values cleanly
  m_scene->registry().emplace<TransformComponent>(
      m_camera_entity, m_editor_camera.position,
      glm::vec3(glm::radians(m_editor_camera.rotation.x),
                glm::radians(m_editor_camera.rotation.y), 0.0f),
      glm::vec3(1.0f, 1.0f, 1.0f));

  m_scene->setActiveRuntimeCamera(m_camera_entity);

  setupInputBindings();
}

void EditorLayer::on_detach() {
  if (m_camera_entity != entt::null) {
    m_scene->registry().destroy(m_camera_entity);
    m_camera_entity = entt::null;
  }
}

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

          float sensitivityX = 0.1f;
          float sensitivityY = 0.1f;

          double delta_x =
              (m_last_mouse_x - details.mouseLandPos.x) * sensitivityX;
          double delta_y =
              (m_last_mouse_y - details.mouseLandPos.y) * sensitivityY;

          m_last_mouse_x = details.mouseLandPos.x;
          m_last_mouse_y = details.mouseLandPos.y;

          m_editor_camera.processMouseMovement(static_cast<float>(delta_x),
                                               static_cast<float>(delta_y));
        }
      });
}

void EditorLayer::on_update(float deltaTime) {
  float cameraSpeed = 10.0f * deltaTime;

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
    auto &camera = m_scene->registry().get<CameraComponent>(m_camera_entity);

    transform.translation = m_editor_camera.position;
    camera.view_matrix = m_editor_camera.getViewMatrix();

    transform.rotation.x = glm::radians(m_editor_camera.rotation.x);
    transform.rotation.y = glm::radians(m_editor_camera.rotation.y);
    transform.rotation.z = glm::radians(m_editor_camera.rotation.z);
  }
}

void EditorLayer::on_ui_render() {
  ImGui::Begin("Scene Hierarchy");
  auto &registry = m_scene->registry();
  auto view = registry.view<TagComponent>();

  for (auto entity : view) {
    const std::string &tag = view.get<TagComponent>(entity).tag;
    std::string label =
        tag + " (ID: " + std::to_string(static_cast<uint32_t>(entity)) + ")";

    ImGuiTreeNodeFlags flags =
        ((m_selected_entity == entity) ? ImGuiTreeNodeFlags_Selected : 0);
    flags |= ImGuiTreeNodeFlags_OpenOnArrow |
             ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx(
        reinterpret_cast<void *>(static_cast<uintptr_t>(entity)), flags, "%s",
        label.c_str());

    if (ImGui::IsItemClicked()) {
      m_selected_entity = entity;
    }
    if (opened) {
      ImGui::TreePop();
    }
  }

  if (ImGui::BeginPopupContextWindow(nullptr,
                                     ImGuiPopupFlags_MouseButtonRight |
                                         ImGuiPopupFlags_NoOpenOverItems)) {
    if (ImGui::MenuItem("Create Empty Entity")) {
      auto newEntity = m_scene->createEntity();
      m_selected_entity = newEntity;
    }
    m_popup_opened = false;
    ImGui::EndPopup();
  }

  ImGui::End();
}

} // namespace ob
