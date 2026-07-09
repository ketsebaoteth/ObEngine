#include "assets/asset_manager.hpp"
#include "log/log.hpp"
#include "rhi/renderer.hpp"
#include "scene/Components.hpp"
#include <vector>

namespace ob {
AssetManager::AssetManager(IRenderer *renderer) : m_renderer(renderer) {
  createCube();
  createPlane();
}
MeshHandle AssetManager::getMesh(const std::string &name) const {
  auto it = m_meshCache.find(name);
  if (it != m_meshCache.end()) {
    return it->second;
  }
  OB_CORE_WARN("AssetManager: Mesh [{}] not found in cache!", name);
  return 0;
}

MeshHandle AssetManager::createPlane() {
  if (m_meshCache.contains("plane")) {
    return m_meshCache["plane"];
  }

  // Centered quad facing straight up along the positive Y-axis (0, 1, 0)
  const glm::vec3 planeNormal = glm::vec3(0.0f, 1.0f, 0.0f);

  std::vector<Vertex> vertices = {
      // Position           // Color               // UV          // Normal
      {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, planeNormal},
      {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, planeNormal},
      {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, planeNormal},
      {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, planeNormal}};

  std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

  MeshHandle handle = m_renderer->uploadMesh(vertices, indices);
  m_meshCache["plane"] = handle;
  return handle;
}

MeshHandle AssetManager::createCube() {
  if (m_meshCache.contains("cube")) {
    return m_meshCache["cube"];
  }

  // 24 Vertices with dedicated, outward-pointing PBR Normals
  std::vector<Vertex> vertices = {
      // --- Front Face (Red, Normal: 0, 0, 1) ---
      {{-0.5f, -0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {0.0f, 0.0f},
       {0.0f, 0.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f},
       {0.0f, 0.0f, 1.0f}},
      {{0.5f, 0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f},
       {0.0f, 0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f}},

      // --- Back Face (Green, Normal: 0, 0, -1) ---
      {{0.5f, -0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f},
       {0.0f, 0.0f, -1.0f}},
      {{0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 1.0f},
       {0.0f, 0.0f, -1.0f}},
      {{-0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 1.0f},
       {0.0f, 0.0f, -1.0f}},
      {{-0.5f, -0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 0.0f},
       {0.0f, 0.0f, -1.0f}},

      // --- Top Face (Blue, Normal: 0, -1, 0 - Inverted for Vulkan coordinate
      // space!) ---
      {{-0.5f, -0.5f, -0.5f},
       {0.0f, 0.0f, 1.0f},
       {0.0f, 0.0f},
       {0.0f, -1.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {0.0f, 1.0f},
       {0.0f, -1.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f},
       {0.0f, -1.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f},
       {0.0f, -1.0f, 0.0f}},

      // --- Bottom Face (Yellow, Normal: 0, 1, 0) ---
      {{-0.5f, 0.5f, 0.5f},
       {1.0f, 1.0f, 0.0f},
       {0.0f, 0.0f},
       {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f},
       {1.0f, 1.0f, 0.0f},
       {0.0f, 1.0f},
       {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f},
       {1.0f, 1.0f, 0.0f},
       {1.0f, 1.0f},
       {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f},
       {1.0f, 1.0f, 0.0f},
       {1.0f, 0.0f},
       {0.0f, 1.0f, 0.0f}},

      // --- Right Face (Cyan, Normal: 1, 0, 0) ---
      {{0.5f, -0.5f, 0.5f},
       {0.0f, 1.0f, 1.0f},
       {0.0f, 0.0f},
       {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f},
       {0.0f, 1.0f, 1.0f},
       {0.0f, 1.0f},
       {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 1.0f},
       {1.0f, 1.0f},
       {1.0f, 0.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f},
       {0.0f, 1.0f, 1.0f},
       {1.0f, 0.0f},
       {1.0f, 0.0f, 0.0f}},

      // --- Left Face (Magenta, Normal: -1, 0, 0) ---
      {{-0.5f, -0.5f, -0.5f},
       {1.0f, 0.0f, 1.0f},
       {0.0f, 0.0f},
       {-1.0f, 0.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f},
       {1.0f, 0.0f, 1.0f},
       {0.0f, 1.0f},
       {-1.0f, 0.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.5f},
       {1.0f, 0.0f, 1.0f},
       {1.0f, 1.0f},
       {-1.0f, 0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f},
       {1.0f, 0.0f, 1.0f},
       {1.0f, 0.0f},
       {-1.0f, 0.0f, 0.0f}}};

  std::vector<uint32_t> indices = {
      0,  1,  2,  2,  3,  0,  // Front
      4,  5,  6,  6,  7,  4,  // Back
      8,  9,  10, 10, 11, 8,  // Top
      12, 13, 14, 14, 15, 12, // Bottom
      16, 17, 18, 18, 19, 16, // Right
      20, 21, 22, 22, 23, 20  // Left
  };

  MeshHandle handle = m_renderer->uploadMesh(vertices, indices);
  m_meshCache["cube"] = handle;
  return handle;
}

MeshHandle AssetManager::loadModel(const std::string &filepath) {
  OB_CORE_WARN("AssetManager: loadModel() is currently a stub for filepath: {}",
               filepath);
  return 0;
}
} // namespace ob
