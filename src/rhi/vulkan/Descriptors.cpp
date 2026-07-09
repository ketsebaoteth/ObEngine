#include "rhi/vulkan_renderer.hpp"
#include <array>
#include <vulkan/vulkan_core.h>

namespace ob {

std::expected<void, std::string> VulkanRenderer::createDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                VK_SHADER_STAGE_FRAGMENT_BIT |
                                VK_SHADER_STAGE_COMPUTE_BIT;
  uboLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding lightLayoutBinding{};
  lightLayoutBinding.binding = 1;
  lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  lightLayoutBinding.descriptorCount = 1;
  lightLayoutBinding.stageFlags =
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
  lightLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding tileLayoutBinding{};
  tileLayoutBinding.binding = 2;
  tileLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  tileLayoutBinding.descriptorCount = 1;
  tileLayoutBinding.stageFlags =
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
  tileLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding depthLayoutBinding{};
  depthLayoutBinding.binding = 3;
  depthLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  depthLayoutBinding.descriptorCount = 1;
  depthLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  depthLayoutBinding.pImmutableSamplers = nullptr;

  std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
      uboLayoutBinding, lightLayoutBinding, tileLayoutBinding,
      depthLayoutBinding};

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                                  &m_globalDescriptorSetLayout) != VK_SUCCESS) {
    return std::unexpected(
        "Failed to create Global Uniform Descriptor Set Layout.");
  }
  return {};
}

std::expected<void, std::string> VulkanRenderer::createUboResources() {
  std::array<VkDescriptorPoolSize, 3> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[1].descriptorCount =
      static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);

  poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) !=
      VK_SUCCESS) {
    return std::unexpected(
        "Unable to initialize Forward+ global descriptor pools.");
  }

  m_frameUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  VkDeviceSize uboSize = sizeof(GlobalUboPayload);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkBufferCreateInfo bufferInfo{.sType =
                                      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = uboSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{.usage = VMA_MEMORY_USAGE_AUTO};
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo resultInfo;
    if (vmaCreateBuffer(
            m_allocator, &bufferInfo, &allocInfo, &m_frameUboBuffers[i].buffer,
            &m_frameUboBuffers[i].allocation, &resultInfo) != VK_SUCCESS) {
      return std::unexpected("Failed to allocate per-frame UBO bindings.");
    }
    m_frameUboBuffers[i].mappedData = resultInfo.pMappedData;
    vmaSetAllocationName(m_allocator, m_frameUboBuffers[i].allocation,
                         "Frame UBO Allocation");
  }

  m_lightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  VkDeviceSize lightBufferSize = MAX_LIGHTS * sizeof(GPUPointLight);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkBufferCreateInfo bufferInfo{.sType =
                                      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = lightBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{.usage = VMA_MEMORY_USAGE_AUTO};
    // NOTE: remember host accessible and permanently mapped so the CPU can
    // write to it every frame instantly!
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo resultInfo;
    if (vmaCreateBuffer(
            m_allocator, &bufferInfo, &allocInfo, &m_lightBuffers[i].buffer,
            &m_lightBuffers[i].allocation, &resultInfo) != VK_SUCCESS) {
      return std::unexpected(
          "Failed to allocate per-frame Light Data SSBO bindings.");
    }
    m_lightBuffers[i].mappedData = resultInfo.pMappedData;
    vmaSetAllocationName(m_allocator, m_lightBuffers[i].allocation,
                         "Frame Light SSBO");
  }

  // TODO: dynamically resize this buffer please future me X) or kaleab if you
  // know wtf this is XD
  VkDeviceSize tileIndicesBufferSize =
      240 * 135 * (1 + MAX_LIGHTS_PER_TILE) * sizeof(uint32_t);

  VkBufferCreateInfo tileBufferInfo{.sType =
                                        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  tileBufferInfo.size = tileIndicesBufferSize;
  tileBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  tileBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo tileAllocInfo{.usage = VMA_MEMORY_USAGE_AUTO};
  tileAllocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (vmaCreateBuffer(m_allocator, &tileBufferInfo, &tileAllocInfo,
                      &m_tileLightIndicesBuffer.buffer,
                      &m_tileLightIndicesBuffer.allocation,
                      nullptr) != VK_SUCCESS) {
    return std::unexpected("Failed to allocate Tile Light Indices SSBO.");
  }
  vmaSetAllocationName(m_allocator, m_tileLightIndicesBuffer.allocation,
                       "Tile Light Indices SSBO");

  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             m_globalDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocSetInfo{};
  allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocSetInfo.descriptorPool = m_descriptorPool;
  allocSetInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocSetInfo.pSetLayouts = layouts.data();

  m_globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_device, &allocSetInfo,
                               m_globalDescriptorSets.data()) != VK_SUCCESS) {
    return std::unexpected(
        "Failed to allocate Forward+ global descriptor sets.");
  }

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo uboBufferInfo{};
    uboBufferInfo.buffer = m_frameUboBuffers[i].buffer;
    uboBufferInfo.offset = 0;
    uboBufferInfo.range = uboSize;

    VkWriteDescriptorSet uboWrite{.sType =
                                      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    uboWrite.dstSet = m_globalDescriptorSets[i];
    uboWrite.dstBinding = 0;
    uboWrite.descriptorCount = 1;
    uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboWrite.pBufferInfo = &uboBufferInfo;

    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = m_lightBuffers[i].buffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = lightBufferSize;

    VkWriteDescriptorSet lightWrite{.sType =
                                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    lightWrite.dstSet = m_globalDescriptorSets[i];
    lightWrite.dstBinding = 1;
    lightWrite.descriptorCount = 1;
    lightWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightWrite.pBufferInfo = &lightBufferInfo;

    VkDescriptorBufferInfo tileBufferInfo{};
    tileBufferInfo.buffer = m_tileLightIndicesBuffer.buffer;
    tileBufferInfo.offset = 0;
    tileBufferInfo.range = tileIndicesBufferSize;

    VkWriteDescriptorSet tileWrite{.sType =
                                       VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    tileWrite.dstSet = m_globalDescriptorSets[i];
    tileWrite.dstBinding = 2;
    tileWrite.descriptorCount = 1;
    tileWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    tileWrite.pBufferInfo = &tileBufferInfo;

    VkDescriptorImageInfo depthBufferInfo{};
    depthBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthBufferInfo.imageView = m_viewportTarget.depthView;
    depthBufferInfo.sampler = m_viewportSampler;

    VkWriteDescriptorSet depthWrite{.sType =
                                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    depthWrite.dstSet = m_globalDescriptorSets[i];
    depthWrite.dstBinding = 3;
    depthWrite.descriptorCount = 1;
    depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    depthWrite.pImageInfo = &depthBufferInfo;

    std::array<VkWriteDescriptorSet, 4> writes = {uboWrite, lightWrite,
                                                  tileWrite, depthWrite};
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  return {};
}

} // namespace ob
