#include "log/log.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "vk_mem_alloc.h"
#include <algorithm>
#include <ranges>

namespace ob {
SwapChainSupportDetails
VulkanRenderer::querySwapChainSupportDetails(VkPhysicalDevice device) {
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface,
                                            &details.capablities);
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
                                       nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
                                         details.formats.data());
  }
  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface,
                                            &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, m_surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  if (availableFormats.empty()) {
    return VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB,
                              VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  }
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}
VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  for (const auto &mode : availablePresentModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
      return mode;
  }

  for (const auto &mode : availablePresentModes) {
    if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
      return mode;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D actualExtent = {static_cast<uint32_t>(m_RendererConfig.width),
                               static_cast<uint32_t>(m_RendererConfig.height)};
    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return actualExtent;
  }
}
std::expected<void, std::string> VulkanRenderer::createSwapChain() {
  SwapChainSupportDetails supportDetails =
      querySwapChainSupportDetails(m_physicalDevice);

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(supportDetails.formats);
  VkPresentModeKHR presnetMode =
      chooseSwapPresentMode(supportDetails.presentModes);
  VkExtent2D extent = chooseSwapExtent(supportDetails.capablities);

  uint32_t imageCount = supportDetails.capablities.minImageCount + 1;
  if (supportDetails.capablities.maxImageCount > 0 &&
      imageCount > supportDetails.capablities.maxImageCount) {
    imageCount = supportDetails.capablities.maxImageCount;
  }

  VkCompositeAlphaFlagBitsKHR compositeAlpha =
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  VkSwapchainCreateInfoKHR createInfo{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  createInfo.surface = m_surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  uint32_t queueFamilyIndices[] = {m_graphicsQueueFamilyIndex,
                                   m_presentQueueFamilyIndex};

  if (m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }
  createInfo.preTransform = supportDetails.capablities.currentTransform;
  createInfo.compositeAlpha = compositeAlpha;
  createInfo.presentMode = presnetMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) !=
      VK_SUCCESS) {
    return std::unexpected("failed to create swapchain");
  }
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
  m_swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount,
                          m_swapchainImages.data());

  m_swapChainImageFormat = surfaceFormat.format;
  m_swapChainExtent = extent;
  return {};
};

std::expected<void, std::string> VulkanRenderer::createSwapchainImageViews() {
  m_swapchainImageViews.resize(m_swapchainImages.size());
  for (const auto [idx, image] : std::views::enumerate(m_swapchainImages)) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m_swapChainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &createInfo, nullptr,
                          &m_swapchainImageViews[idx]) != VK_SUCCESS) {
      return std::unexpected("Failed to create image views");
    }
  }
  return {};
};

std::expected<void, std::string> VulkanRenderer::createDepthResources() {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = m_swapChainExtent.width;
  imageInfo.extent.height = m_swapChainExtent.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = m_depthFormat;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &m_depthImage,
                     &m_depthAllocation, nullptr) != VK_SUCCESS) {
    return std::unexpected("Vulkan Resource Error: Failed to allocate hardware "
                           "depth image buffer via VMA.");
  }

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_depthImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = m_depthFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) !=
      VK_SUCCESS) {
    return std::unexpected("Vulkan Resource Error: Failed to instantiate "
                           "concrete target view for depth buffer.");
  }

  return {};
}
void VulkanRenderer::cleanupSwapchain() {
  for (auto framebuffer : m_swapChainFramebuffers) {
    if (framebuffer != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
  }
  m_swapChainFramebuffers.clear();

  if (m_depthImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    m_depthImageView = VK_NULL_HANDLE;
  }
  if (m_depthImage != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, m_depthImage, m_depthAllocation);
    m_depthImage = VK_NULL_HANDLE;
    m_depthAllocation = VK_NULL_HANDLE;
  }

  for (auto imageView : m_swapchainImageViews) {
    if (imageView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_device, imageView, nullptr);
    }
  }
  m_swapchainImageViews.clear();

  if (m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }
}

} // namespace ob
