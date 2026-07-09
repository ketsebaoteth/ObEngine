#include "rhi/vulkan_renderer.hpp"
#include <cstring>

namespace ob {
void VulkanRenderer::updateLightData(std::span<const GPUPointLight> lights) {
  uint32_t lightCount = static_cast<uint32_t>(
      std::min(lights.size(), static_cast<size_t>(MAX_LIGHTS)));

  std::memcpy(m_lightBuffers[m_currentFrame].mappedData, lights.data(),
              lightCount * sizeof(GPUPointLight));

  if (lightCount < MAX_LIGHTS) {
    uint32_t remainingLights = MAX_LIGHTS - lightCount;
    void *offsetPtr =
        static_cast<char *>(m_lightBuffers[m_currentFrame].mappedData) +
        (lightCount * sizeof(GPUPointLight));
    std::memset(offsetPtr, 0, remainingLights * sizeof(GPUPointLight));
  }
}
} // namespace ob
