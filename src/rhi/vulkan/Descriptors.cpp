#include "rhi/vulkan_renderer.hpp"

namespace ob {

std::expected<void, std::string> VulkanRenderer::createDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  uboLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

  if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                                  &m_globalDescriptorSetLayout) != VK_SUCCESS) {
    return std::unexpected(
        "Failed to create Global Uniform Descriptor Set Layout.");
  }
  return {};
}

std::expected<void, std::string> VulkanRenderer::createUboResources() {
  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) !=
      VK_SUCCESS) {
    return std::unexpected(
        "Unable to initialize Renderer global descriptor allocations pools.");
  }

  m_frameUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  VkDeviceSize bufferSize = sizeof(GlobalUboPayload);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo resultInfo;
    if (vmaCreateBuffer(
            m_allocator, &bufferInfo, &allocInfo, &m_frameUboBuffers[i].buffer,
            &m_frameUboBuffers[i].allocation, &resultInfo) != VK_SUCCESS) {
      return std::unexpected(
          "Failed to allocate per-frame uniform buffer bindings.");
    }
    m_frameUboBuffers[i].mappedData = resultInfo.pMappedData;
  }

  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             m_globalDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  m_globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_device, &allocInfo,
                               m_globalDescriptorSets.data()) != VK_SUCCESS) {
    return std::unexpected("Failed to bind layout configurations to uniform "
                           "descriptor instances.");
  }

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_frameUboBuffers[i].buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(GlobalUboPayload);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_globalDescriptorSets[i];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
  }

  return {};
}

} // namespace ob
