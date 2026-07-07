#pragma once
#include "entt/entity/fwd.hpp"
#include "scene/Components.hpp"
#include <entt/entt.hpp>

namespace ob {
class Scene {
public:
  Scene() = default;
  ~Scene() = default;

  entt::entity createEntity() { return m_registry.create(); }
  void destroyEntity(entt::entity entity) { m_registry.destroy(entity); }

  void setActiveRuntimeCamera(entt::entity cameraEntity) {
    // Clear old active selection flags
    auto view = m_registry.view<CameraComponent>();
    for (auto entity : view) {
      view.get<CameraComponent>(entity).is_active = false;
    }

    if (m_registry.all_of<CameraComponent>(cameraEntity)) {
      m_registry.get<CameraComponent>(cameraEntity).is_active = true;
      m_active_runtime_camera = cameraEntity;
    }
  }

  bool isUsingEditorCamera() { return m_is_using_editor_camera; }

  entt::entity getActiveRuntimeCamera() { return m_active_runtime_camera; }
  entt::registry &registry() { return m_registry; }
  const entt::registry &registry() const { return m_registry; }

private:
  entt::registry m_registry;
  entt::entity m_active_runtime_camera{entt::null};
  bool m_is_using_editor_camera = false;
};
} // namespace ob
