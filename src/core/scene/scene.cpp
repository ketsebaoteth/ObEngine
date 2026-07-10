#include "scene/Scene.hpp"
#include "rhi/vulkan_renderer.hpp"

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

// In scene.cpp
// In scene.cpp
void Scene::draw(IRenderer *renderer, int width, int height) {
  // --- 1. GATHER ALL ACTIVE LIGHT SOURCES ---
  std::vector<GPUPointLight> gpuLights;
  auto lightView = m_registry.view<TransformComponent, PointLightComponent>();

  bool hasShadowCaster = false;
  for (auto entity : lightView) {
    const auto &transform = m_registry.get<TransformComponent>(entity);
    const auto &light = m_registry.get<PointLightComponent>(entity);

    gpuLights.push_back({.position = transform.translation,
                         .range = light.range,
                         .color = light.color,
                         .intensity = light.intensity});

    // --- NEW FORWARD+: UPDATE SHADOW MATRICES ON THE GPU ---
    // If we find our first active point light, compile its 6 shadow matrices!
    if (!hasShadowCaster) {
      renderer->updateShadowData(transform.translation, light.range);
      hasShadowCaster = true;
    }
  }

  // Upload light array to GPU SSBO
  renderer->updateLightData(gpuLights);

  // If no point lights exist, we pass a dummy shadow matrix so Vulkan doesn't
  // complain
  if (!hasShadowCaster) {
    renderer->updateShadowData(glm::vec3(0.0f, 1000.0f, 0.0f), 1.0f);
  }

  // --- 2. GATHER GEOMETRY RENDER ITEMS ---
  std::vector<RenderItem> renderQueue;
  auto view = m_registry.view<TransformComponent, MeshComponent>();
  for (auto entity : view) {
    const auto &transform = m_registry.get<TransformComponent>(entity);
    const auto &mesh = m_registry.get<MeshComponent>(entity);

    RenderItem item{};
    item.handle = mesh.handle;
    item.transform = transform.getTransform();

    // Copy dynamic material properties from EnTT to RenderItem
    if (m_registry.all_of<PBRMaterialComponent>(entity)) {
      const auto &mat = m_registry.get<PBRMaterialComponent>(entity);

      item.baseColor = mat.vectorParameters.at("baseColor");
      item.metallic = mat.scalarParameters.at("metallic");
      item.roughness = mat.scalarParameters.at("roughness");
      item.emissionStrength = mat.scalarParameters.at("emissionStrength");
      item.emissionColor = glm::vec3(mat.vectorParameters.at("emissionColor"));
    }

    renderQueue.push_back(item);
  }

  // --- 3. CAMERA VIEW/PROJECTION CALCULATIONS ---
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

  // 4. Submit the synchronized command buffer to the GPU!
  renderer->present(renderQueue, viewMatrix, projectionMatrix);
}

} // namespace ob
