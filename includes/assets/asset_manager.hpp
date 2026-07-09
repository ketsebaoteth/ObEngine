#pragma once
#include "rhi/renderer.hpp"
#include <string>
#include <unordered_map>

namespace ob {
class AssetManager {
public:
  AssetManager(IRenderer *renderer);
  ~AssetManager() = default;

  MeshHandle createCube();
  MeshHandle createPlane();

  MeshHandle loadModel(const std::string &filepath);
  [[nodiscard]] MeshHandle getMesh(const std::string &name) const;

private:
  IRenderer *m_renderer;
  std::unordered_map<std::string, MeshHandle> m_meshCache;
};
} // namespace ob
