#include "glm/ext/vector_float2.hpp"
#include "log/log.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "scene/Components.hpp"
#include <array>
#include <backends/imgui_impl_vulkan.h>
#include <cstring>
#include <expected>
#include <glm/glm.hpp>
#include <imgui.h> // Included for access to ImGui::GetDrawData()

namespace ob {

// Records geometry data directly into your custom offscreen render target
void recordGeometryPass(
    VkCommandBuffer commandBuffer, VkRenderPass renderPass,
    VkFramebuffer framebuffer, VkExtent2D extent, VkPipeline graphicsPipeline,
    VkPipelineLayout pipelineLayout, std::span<const RenderItem> renderQueue,
    const std::unordered_map<MeshHandle, VulkanRenderer::VulkanMeshBackend>
        &meshCache,
    VkDescriptorSet globalDescriptorSet) {

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.13f, 0.13f, 0.13f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.framebuffer = framebuffer;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = extent;
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    graphicsPipeline);

  VkViewport viewport{0.0f,
                      0.0f,
                      static_cast<float>(extent.width),
                      static_cast<float>(extent.height),
                      0.0f,
                      1.0f};
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{{0, 0}, extent};
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, 1, &globalDescriptorSet, 0,
                          nullptr);

  for (const auto &item : renderQueue) {
    auto it = meshCache.find(item.handle);
    if (it == meshCache.end())
      continue;

    const auto &backendMesh = it->second;
    if (backendMesh.vertexBuffer != VK_NULL_HANDLE) {
      PushConstantPayload push{};
      push.model = item.transform;
      push.baseColor = item.baseColor;
      push.metallic = item.metallic;
      push.roughness = item.roughness;
      push.emissionColor = item.emissionColor;
      push.emissionStrength = item.emissionStrength;

      vkCmdPushConstants(commandBuffer, pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT |
                             VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(PushConstantPayload), &push);

      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &backendMesh.vertexBuffer,
                             offsets);

      if (backendMesh.indexBuffer != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(commandBuffer, backendMesh.indexBuffer, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, backendMesh.indexCount, 1, 0, 0, 0);
      } else {
        vkCmdDraw(commandBuffer, backendMesh.vertexCount, 1, 0, 0);
      }
    }
  }
  vkCmdEndRenderPass(commandBuffer);
}

void VulkanRenderer::present(std::span<const RenderItem> renderQueue,
                             const glm::mat4 &view, const glm::mat4 &proj) {
  vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE,
                  UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      m_device, m_swapchain, UINT64_MAX,
      m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    OB_CORE_ERROR("Vulkan Execution Error: Critical failure acquiring next "
                  "swapchain image frame.");
    return;
  }

  GlobalUboPayload payload{
      .view = view,
      .proj = proj,
      .screenSize = glm::vec2(static_cast<float>(m_viewportTarget.width),
                              static_cast<float>(m_viewportTarget.height))};
  std::memcpy(m_frameUboBuffers[m_currentFrame].mappedData, &payload,
              sizeof(GlobalUboPayload));

  vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

  VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
    OB_CORE_ERROR("Vulkan Execution Error: Failed to begin command buffer "
                  "recording stream.");
    return;
  }

  // =====================================================================
  // --- FORWARD+: TILE-BASED LIGHT CULLING COMPUTE PASS ---
  // =====================================================================

  // --- TRANSITION 1: Depth Attachment -> Shader Read (For Compute) ---
  VkImageMemoryBarrier depthReadBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  depthReadBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthReadBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthReadBarrier.image = m_viewportTarget.depthImage;
  depthReadBarrier.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                       .levelCount = 1,
                                       .layerCount = 1};
  depthReadBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depthReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthReadBarrier);

  // 1. Bind Compute Pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

  // 2. Bind Unified Descriptor Set
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_computePipelineLayout, 0, 1,
                          &m_globalDescriptorSets[m_currentFrame], 0, nullptr);

  // 3. Dispatch the culler across the tile grid
  uint32_t groupCountX = (m_viewportTarget.width + 15) / 16;
  uint32_t groupCountY = (m_viewportTarget.height + 15) / 16;
  vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

  // --- TRANSITION 2: Shader Read -> Depth Attachment (For Geometry Pass) ---
  VkImageMemoryBarrier depthWriteBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  depthWriteBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  depthWriteBarrier.newLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depthWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  depthWriteBarrier.image = m_viewportTarget.depthImage;
  depthWriteBarrier.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                        .levelCount = 1,
                                        .layerCount = 1};
  depthWriteBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  depthWriteBarrier.dstAccessMask =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       0, 0, nullptr, 0, nullptr, 1, &depthWriteBarrier);

  // --- TRANSITION 3: Compute indices list write completed sync barrier ---
  VkBufferMemoryBarrier bufferBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  bufferBarrier.srcAccessMask =
      VK_ACCESS_SHADER_WRITE_BIT; // Compute wrote indices
  bufferBarrier.dstAccessMask =
      VK_ACCESS_SHADER_READ_BIT; // Pixel shader will read them
  bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bufferBarrier.buffer = m_tileLightIndicesBuffer.buffer;
  bufferBarrier.offset = 0;
  bufferBarrier.size = VK_WHOLE_SIZE;

  vkCmdPipelineBarrier(
      cmd,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Source stage
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // Destination stage
      0, 0, nullptr, 1, &bufferBarrier,      // Wait on buffer
      0, nullptr);

  // =====================================================================

  // --- PASS 1: RENDER SCENE GEOMETRY TO OFFSCREEN FRAMEBUFFER ---
  VkExtent2D offscreenExtent{m_viewportTarget.width, m_viewportTarget.height};
  recordGeometryPass(cmd, m_renderPass, m_viewportTarget.framebuffer,
                     offscreenExtent, m_graphicsPipeline, m_pipelineLayout,
                     renderQueue, m_meshes,
                     m_globalDescriptorSets[m_currentFrame]);

  // --- PIPELINE BARRIER: MEMORY TRANSITION FOR IMGUI SAMPLING ---
  VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_viewportTarget.colorImage;
  barrier.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .baseMipLevel = 0,
                              .levelCount = 1,
                              .baseArrayLayer = 0,
                              .layerCount = 1};
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // --- PASS 2: RENDER IMGUI DIRECTLY ONTO SWAPCHAIN SWAP BUFFER ---
  VkRenderPassBeginInfo swapchainPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  swapchainPassInfo.renderPass = m_renderPass;
  swapchainPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];
  swapchainPassInfo.renderArea.offset = {0, 0};
  swapchainPassInfo.renderArea.extent = m_swapChainExtent;

  std::array<VkClearValue, 2> swapClearValues{};
  swapClearValues[0].color = {
      {0.02f, 0.04f, 0.06f, 1.0f}}; // Main window clear color
  swapClearValues[1].depthStencil = {1.0f, 0};
  swapchainPassInfo.clearValueCount =
      static_cast<uint32_t>(swapClearValues.size());
  swapchainPassInfo.pClearValues = swapClearValues.data();

  vkCmdBeginRenderPass(cmd, &swapchainPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  ImDrawData *drawData = ImGui::GetDrawData();
  if (drawData) {
    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
  }

  vkCmdEndRenderPass(cmd);

  // --- REVERSE PIPELINE BARRIER: RESTORE OFFSCREEN LAYOUT FOR NEXT OVERWRITE
  // CYCLE ---
  barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.newLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // FIX: Never use UNDEFINED as a
                                                // newLayout destination!
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    OB_CORE_ERROR("Vulkan Execution Error: Failed to finalize frame command "
                  "compilation.");
  }

  // --- SUBMISSION AND PRESENTATION STAGE ---
  VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[imageIndex]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(m_graphics_queue, 1, &submitInfo,
                    m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
    OB_CORE_ERROR("Vulkan Execution Error: Failed to submit frame drawing "
                  "commands to hardware queue.");
  }

  VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  VkSwapchainKHR swapChains[] = {m_swapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(m_present_queue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    OB_CORE_WARN("Swapchain out of date or suboptimal. Re-anchoring via "
                 "incoming event loop.");
  } else if (result != VK_SUCCESS) {
    OB_CORE_ERROR("Vulkan Execution Error: Critical failure presenting "
                  "finalized frame to display surface.");
  }

  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter,
                                        VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }
  OB_CORE_ERROR("Vulkan Memory Error: Failed to find a suitable physical "
                "device memory type index.");
  return 0;
}

// In vulkan_renderer.cpp
std::expected<void, std::string>
VulkanRenderer::createOffscreenRenderTarget(uint32_t width, uint32_t height) {
  destroyOffscreenRenderTarget();

  m_viewportTarget.width = width;
  m_viewportTarget.height = height;

  VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = {width, height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = m_swapChainImageFormat;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo{};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocCreateInfo,
                     &m_viewportTarget.colorImage,
                     &m_viewportTarget.colorAllocation,
                     nullptr) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan Resource Error: Failed to allocate offscreen color image.");
  }

  VkImageViewCreateInfo viewInfo{.sType =
                                     VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = m_viewportTarget.colorImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = m_swapChainImageFormat;
  viewInfo.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .levelCount = 1,
                               .layerCount = 1};

  if (vkCreateImageView(m_device, &viewInfo, nullptr,
                        &m_viewportTarget.colorView) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan View Error: Failed to create offscreen color view mapping.");
  }

  imageInfo.format = VK_FORMAT_D32_SFLOAT;
  imageInfo.usage =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocCreateInfo,
                     &m_viewportTarget.depthImage,
                     &m_viewportTarget.depthAllocation,
                     nullptr) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan Resource Error: Failed to allocate offscreen depth buffer.");
  }

  viewInfo.image = m_viewportTarget.depthImage;
  viewInfo.format = VK_FORMAT_D32_SFLOAT;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  if (vkCreateImageView(m_device, &viewInfo, nullptr,
                        &m_viewportTarget.depthView) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan View Error: Failed to create offscreen depth view mapping.");
  }

  std::array<VkImageView, 2> attachments = {m_viewportTarget.colorView,
                                            m_viewportTarget.depthView};

  VkFramebufferCreateInfo fbInfo{.sType =
                                     VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fbInfo.renderPass = m_renderPass;
  fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  fbInfo.pAttachments = attachments.data();
  fbInfo.width = width;
  fbInfo.height = height;
  fbInfo.layers = 1;

  if (vkCreateFramebuffer(m_device, &fbInfo, nullptr,
                          &m_viewportTarget.framebuffer) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan Framebuffer Error: Failed to compile offscreen framebuffer.");
  }

  // --- 4. SAMPLER CREATION (One-time only) ---
  if (m_viewportSampler == VK_NULL_HANDLE) {
    VkSamplerCreateInfo samplerInfo{.sType =
                                        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_viewportSampler) !=
        VK_SUCCESS) {
      return std::unexpected(
          "Vulkan Sampler Error: Failed to create offscreen sampler.");
    }
  }

  // --- 5. DEFERRED IMGUI REGISTRATION (Safe order check!) ---
  if (ImGui::GetCurrentContext() != nullptr &&
      ImGui::GetIO().BackendRendererUserData != nullptr) {
    register_imgui_viewport_texture();
  } else {
    m_viewportTarget.imguiDescriptorSet = VK_NULL_HANDLE;
  }
  if (!m_globalDescriptorSets.empty()) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      VkDescriptorImageInfo depthBufferInfo{};
      depthBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      depthBufferInfo.imageView = m_viewportTarget.depthView;
      depthBufferInfo.sampler = m_viewportSampler;

      VkWriteDescriptorSet depthWrite{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      depthWrite.dstSet = m_globalDescriptorSets[i];
      depthWrite.dstBinding = 3;
      depthWrite.descriptorCount = 1;
      depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      depthWrite.pImageInfo = &depthBufferInfo;

      vkUpdateDescriptorSets(m_device, 1, &depthWrite, 0, nullptr);
    }
  }
  return {};
}

void VulkanRenderer::destroyOffscreenRenderTarget() {
  if (m_device == VK_NULL_HANDLE) {
    return;
  }

  if (m_viewportTarget.framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(m_device, m_viewportTarget.framebuffer, nullptr);
    m_viewportTarget.framebuffer = VK_NULL_HANDLE;
  }

  if (m_viewportTarget.depthView != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, m_viewportTarget.depthView, nullptr);
    m_viewportTarget.depthView = VK_NULL_HANDLE;
  }
  if (m_viewportTarget.depthImage != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, m_viewportTarget.depthImage,
                    m_viewportTarget.depthAllocation);
    m_viewportTarget.depthImage = VK_NULL_HANDLE;
    m_viewportTarget.depthAllocation = VK_NULL_HANDLE;
  }

  if (m_viewportTarget.colorView != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, m_viewportTarget.colorView, nullptr);
    m_viewportTarget.colorView = VK_NULL_HANDLE;
  }
  if (m_viewportTarget.colorImage != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, m_viewportTarget.colorImage,
                    m_viewportTarget.colorAllocation);
    m_viewportTarget.colorImage = VK_NULL_HANDLE;
    m_viewportTarget.colorAllocation = VK_NULL_HANDLE;
  }

  if (m_viewportTarget.imguiDescriptorSet != VK_NULL_HANDLE) {
    if (ImGui::GetCurrentContext() != nullptr &&
        ImGui::GetIO().BackendRendererUserData != nullptr) {
      ImGui_ImplVulkan_RemoveTexture(m_viewportTarget.imguiDescriptorSet);
    }
    m_viewportTarget.imguiDescriptorSet = VK_NULL_HANDLE;
  }
}

} // namespace ob
