#include "log/log.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "utils/io.hpp"

namespace ob {
std::expected<void, std::string> VulkanRenderer::createShadowResources() {
  uint32_t shadowResolution = 2048;

  VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = {shadowResolution, shadowResolution, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 6;
  imageInfo.format = VK_FORMAT_D32_SFLOAT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO};
  allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocCreateInfo,
                     &m_shadowCubemapImage, &m_shadowCubemapAllocation,
                     nullptr) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan Resource Error: Failed to allocate 3D Shadow Depth Cubemap.");
  }

  VkImageViewCreateInfo viewInfo{.sType =
                                     VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = m_shadowCubemapImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; // Set view type to CUBE!
  viewInfo.format = VK_FORMAT_D32_SFLOAT;
  viewInfo.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                               .levelCount = 1,
                               .layerCount = 6}; // Covers all 6 layers!

  if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_shadowCubemapView) !=
      VK_SUCCESS) {
    return std::unexpected(
        "Vulkan View Error: Failed to create 3D Shadow view mapping.");
  }

  if (m_shadowSampler == VK_NULL_HANDLE) {
    VkSamplerCreateInfo samplerInfo{.sType =
                                        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_shadowSampler) !=
        VK_SUCCESS) {
      return std::unexpected(
          "Vulkan Sampler Error: Failed to create shadow sampler.");
    }
  }

  return {};
}

std::expected<void, std::string> VulkanRenderer::createShadowRenderPass() {
  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = VK_FORMAT_D32_SFLOAT;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthReference{};
  depthReference.attachment = 0;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 0;
  subpass.pColorAttachments = nullptr;
  subpass.pDepthStencilAttachment = &depthReference;

  uint32_t viewMask = 0b00111111;
  uint32_t correlationMask = 0b00111111;

  VkRenderPassMultiviewCreateInfo multiviewInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
  multiviewInfo.subpassCount = 1;
  multiviewInfo.pViewMasks = &viewMask;
  multiviewInfo.correlationMaskCount = 1;
  multiviewInfo.pCorrelationMasks = &correlationMask;

  VkRenderPassCreateInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  renderPassInfo.pNext = &multiviewInfo; // Link multiview creating info!
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &depthAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr,
                         &m_shadowRenderPass) != VK_SUCCESS) {
    return std::unexpected("Vulkan Error: Failed to compile hardware Multiview "
                           "Shadow Render Pass.");
  }

  uint32_t shadowResolution = 2048;
  VkFramebufferCreateInfo fbInfo{.sType =
                                     VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fbInfo.renderPass = m_shadowRenderPass;
  fbInfo.attachmentCount = 1;
  fbInfo.pAttachments = &m_shadowCubemapView;
  fbInfo.width = shadowResolution;
  fbInfo.height = shadowResolution;
  fbInfo.layers = 1;

  if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_shadowFramebuffer) !=
      VK_SUCCESS) {
    return std::unexpected(
        "Vulkan Error: Failed to link shadow views to framebuffer context.");
  }

  OB_CORE_INFO("Vulkan Multiview Shadow Render Pass successfully compiled.");
  return {};
}
// In vulkan_renderer.cpp
std::expected<void, std::string> VulkanRenderer::createShadowPipeline() {
  std::string vertPath = io::ResolveAssetPath("shaders/shadow.vert.spv");
  auto vertShaderModuleRes = createShaderModule(vertPath);
  if (!vertShaderModuleRes)
    return std::unexpected("Shadow Vert Error: " + vertShaderModuleRes.error());
  VkShaderModule vertShaderModule = vertShaderModuleRes.value();

  std::string fragPath = io::ResolveAssetPath("shaders/shadow.frag.spv");
  auto fragShaderModuleRes = createShaderModule(fragPath);
  if (!fragShaderModuleRes) {
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    return std::unexpected("Shadow Frag Error: " + fragShaderModuleRes.error());
  }
  VkShaderModule fragShaderModule = fragShaderModuleRes.value();

  // --- 1. DEFINE DESCRIPTOR SET LAYOUT ---
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT; // Only vertex shader needs the matrices!

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

  if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                                  &m_shadowDescriptorSetLayout) != VK_SUCCESS) {
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    return std::unexpected(
        "Vulkan Error: Failed to create Shadow Descriptor Set Layout.");
  }

  // --- 2. COMPILE SHADOW PIPELINE LAYOUT ---
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(ShadowPushConstant);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_shadowDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr,
                             &m_shadowPipelineLayout) != VK_SUCCESS) {
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    return std::unexpected(
        "Vulkan Error: Failed to compile Shadow Pipeline Layout.");
  }

  // --- 3. CONFIGURE GRAPHICS STATES ---
  VkPipelineShaderStageCreateInfo vertStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertShaderModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragShaderModule;
  fragStage.pName = "main";

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStage,
                                                                 fragStage};

  auto bindingDesc = Vertex::getBindingDescription();
  auto attribDesc = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attribDesc.size());
  vertexInputInfo.pVertexAttributeDescriptions = attribDesc.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamicStateInfo.dynamicStateCount =
      static_cast<uint32_t>(dynamicStates.size());
  dynamicStateInfo.pDynamicStates = dynamicStates.data();

  // --- ACTIVATE DEPTH BIAS TO PREVENT SHADOW ACNE! ---
  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace =
      VK_FRONT_FACE_CLOCKWISE; // Keeps winding-order aligned!

  // Enable depth biasing (The acne killer!)
  rasterizer.depthBiasEnable = VK_TRUE;
  rasterizer.depthBiasConstantFactor = 1.25f; // Constant depth offset
  rasterizer.depthBiasSlopeFactor = 1.75f;    // Slope-scaled depth offset

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

  // COLOR BLENDING IS 0 (Depth-only pass!)
  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  colorBlending.attachmentCount = 0;
  colorBlending.pAttachments = nullptr;

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicStateInfo;
  pipelineInfo.layout = m_shadowPipelineLayout;
  pipelineInfo.renderPass = m_shadowRenderPass;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &m_shadowPipeline) != VK_SUCCESS) {
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    return std::unexpected(
        "Vulkan Error: Failed to compile Shadow Graphics Pipeline.");
  }

  vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
  vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
  return {};
}
std::expected<void, std::string> VulkanRenderer::createShadowUboResources() {
  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr,
                             &m_shadowDescriptorPool) != VK_SUCCESS) {
    return std::unexpected("Unable to initialize Shadow Descriptor Pool.");
  }

  m_shadowUboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  VkDeviceSize bufferSize = 6 * sizeof(glm::mat4);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkBufferCreateInfo bufferInfo{.sType =
                                      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{.usage = VMA_MEMORY_USAGE_AUTO};
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo resultInfo;
    if (vmaCreateBuffer(
            m_allocator, &bufferInfo, &allocInfo, &m_shadowUboBuffers[i].buffer,
            &m_shadowUboBuffers[i].allocation, &resultInfo) != VK_SUCCESS) {
      return std::unexpected(
          "Failed to allocate per-frame Shadow Matrix UBO bindings.");
    }
    m_shadowUboBuffers[i].mappedData = resultInfo.pMappedData;
    vmaSetAllocationName(m_allocator, m_shadowUboBuffers[i].allocation,
                         "Frame Shadow Matrix UBO");
  }

  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             m_shadowDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocSetInfo{};
  allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocSetInfo.descriptorPool = m_shadowDescriptorPool;
  allocSetInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocSetInfo.pSetLayouts = layouts.data();

  m_shadowDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_device, &allocSetInfo,
                               m_shadowDescriptorSets.data()) != VK_SUCCESS) {
    return std::unexpected("Failed to allocate Shadow descriptor sets.");
  }

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_shadowUboBuffers[i].buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    VkWriteDescriptorSet uboWrite{.sType =
                                      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    uboWrite.dstSet = m_shadowDescriptorSets[i];
    uboWrite.dstBinding = 0;
    uboWrite.descriptorCount = 1;
    uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &uboWrite, 0, nullptr);
  }

  return {};
}
void VulkanRenderer::updateShadowData(glm::vec3 lightPos, float lightRange) {
  glm::mat4 shadowProj =
      glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, lightRange);
  // shadowProj[1][1] *= -1.0f;
  m_shadowLightPos = lightPos;
  m_shadowLightRange = lightRange;

  std::array<glm::mat4, 6> shadowViews = {
      glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)), // +X
      glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)), // -X
      glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f)), // +Y
      glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f)), // -Y
      glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)), // +Z
      glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)) // -Z
  };

  std::array<glm::mat4, 6> shadowMatrices;
  for (int i = 0; i < 6; i++) {
    shadowMatrices[i] = shadowProj * shadowViews[i];
  }

  std::memcpy(m_shadowUboBuffers[m_currentFrame].mappedData,
              shadowMatrices.data(), 6 * sizeof(glm::mat4));
}
} // namespace ob
