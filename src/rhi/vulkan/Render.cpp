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
  clearValues[0].color = {{0.02f, 0.04f, 0.06f, 1.0f}};
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
      vkCmdPushConstants(commandBuffer, pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                         &item.transform);

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

  GlobalUboPayload payload{.view = view, .proj = proj};
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

  // --- 1. ALLOCATE COLOR RESOURCE (Using VMA) ---
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

  // Create Color View
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

  // --- 2. ALLOCATE DEPTH RESOURCE (Using VMA) ---
  imageInfo.format = VK_FORMAT_D32_SFLOAT;
  imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocCreateInfo,
                     &m_viewportTarget.depthImage,
                     &m_viewportTarget.depthAllocation,
                     nullptr) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan Resource Error: Failed to allocate offscreen depth buffer.");
  }

  // Create Depth View
  viewInfo.image = m_viewportTarget.depthImage;
  viewInfo.format = VK_FORMAT_D32_SFLOAT;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  if (vkCreateImageView(m_device, &viewInfo, nullptr,
                        &m_viewportTarget.depthView) != VK_SUCCESS) {
    return std::unexpected(
        "Vulkan View Error: Failed to create offscreen depth view mapping.");
  }

  // --- 3. CREATE FRAMEBUFFER ---
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
