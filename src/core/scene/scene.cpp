#include "scene/Scene.hpp"
#include "rhi/renderer.hpp"

namespace ob {
Scene::Scene(IRenderer *renderer) : m_renderer(renderer) {}

Scene::~Scene() {
  if (m_renderer) {
    auto view = m_registry.view<MeshComponent>();
    for (auto entity : view) {
      auto &mesh = view.get<MeshComponent>(entity);
      if (mesh.handle != 0) {
        m_renderer->destroyMesh(mesh.handle);
        mesh.handle = 0;
      }
    }
  }
}

void Scene::draw(IRenderer *renderer, int width, int height) {
  std::vector<RenderItem> renderQueue;

  auto view = m_registry.view<TransformComponent, MeshComponent>();
  for (auto entity : view) {
    const auto &transform = m_registry.get<TransformComponent>(entity);
    const auto &mesh = m_registry.get<MeshComponent>(entity);
    renderQueue.push_back(
        {.handle = mesh.handle, .transform = transform.getTransform()});
  }

  glm::mat4 viewMatrix(1.0f);
  glm::mat4 projectionMatrix(1.0f);
  float aspectRatio =
      (height > 0) ? (static_cast<float>(width) / static_cast<float>(height))
                   : 1.0f;

  entt::entity activeCam = getActiveRuntimeCamera();
  if (activeCam != entt::null) {
    if (m_registry.all_of<TransformComponent, CameraComponent>(activeCam)) {
      const auto &camera = m_registry.get<CameraComponent>(activeCam);
      viewMatrix = camera.view_matrix;
      projectionMatrix = camera.getProjection(aspectRatio);
    }
  }

  renderer->present(renderQueue, viewMatrix, projectionMatrix);
};

} // namespace ob
