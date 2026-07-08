// In vulkan_renderer.hpp
#pragma once
#include "renderer.hpp"
#include <expected>
#include <scene/Components.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// Lightweight Vulkan & Memory Allocator Headers
#include <volk.h>
#include <vulkan/vulkan_core.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

namespace ob {

// --- Core Data Structures ---

struct OffscreenRenderTarget {
  VkImage colorImage = VK_NULL_HANDLE;
  VmaAllocation colorAllocation = VK_NULL_HANDLE; // Changed from VkDeviceMemory
  VkImageView colorView = VK_NULL_HANDLE;

  VkImage depthImage = VK_NULL_HANDLE;
  VmaAllocation depthAllocation = VK_NULL_HANDLE; // Changed from VkDeviceMemory
  VkImageView depthView = VK_NULL_HANDLE;

  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;

  VkDescriptorSet imguiDescriptorSet = VK_NULL_HANDLE;

  uint32_t width = 0;
  uint32_t height = 0;
};

struct VulkanContext {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct GlobalUboPayload {
  glm::mat4 view;
  glm::mat4 proj;
};

// --- Vulkan Renderer Implementation ---

class VulkanRenderer : public IRenderer {
public:
  struct VulkanMeshBackend {
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VmaAllocation vertexAllocation{VK_NULL_HANDLE};
    uint32_t vertexCount{0};

    VkBuffer indexBuffer{VK_NULL_HANDLE};
    VmaAllocation indexAllocation{VK_NULL_HANDLE};
    uint32_t indexCount{0};
  };

public:
  VulkanRenderer() = default;
  ~VulkanRenderer() override;

  // IRenderer Interface Overrides
  std::expected<void, std::string> init(const RendererConfig &config) override;
  void shutdown() override;
  void resize(uint32_t w, uint32_t h) override;
  void present(std::span<const RenderItem> renderQueue, const glm::mat4 &view,
               const glm::mat4 &proj) override;

  [[nodiscard]] MeshHandle
  uploadMesh(std::span<const Vertex> vertices,
             std::span<const uint32_t> indices) override;
  void destroyMesh(MeshHandle handle) override;

  void waitDeviceIdle() override;
  void
  register_imgui_viewport_texture() override; // Implementation moved to .cpp!

  // Dynamic Viewport & UI Getters
  [[nodiscard]] void *get_viewport_texture_id() const override {
    return static_cast<void *>(m_viewportTarget.imguiDescriptorSet);
  }
  [[nodiscard]] VkImageView getViewportImageView() const override {
    return m_viewportTarget.colorView;
  }
  [[nodiscard]] VkSampler getViewportSampler() const override {
    return m_viewportSampler;
  }
  [[nodiscard]] VulkanContext get_vulkan_context() const override;

  // Hardware Getters
  [[nodiscard]] VkDevice getDevice() const { return m_device; }
  [[nodiscard]] VmaAllocator getAllocator() { return m_allocator; }
  [[nodiscard]] VkInstance getInstance() const { return m_instance; }
  [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const {
    return m_physicalDevice;
  }
  [[nodiscard]] VkQueue getGraphicsQueue() const { return m_graphics_queue; }
  [[nodiscard]] VkRenderPass getRenderPass() const { return m_renderPass; }
  [[nodiscard]] VkCommandBuffer getCurrentCommandBuffer();

  // Public Utility Methods
  std::expected<void, std::string> createOffscreenRenderTarget(uint32_t width,
                                                               uint32_t height);
  void destroyOffscreenRenderTarget();
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

private:
  VmaAllocator m_allocator;
  // Buffer allocation helpers
  std::expected<void, std::string>
  uploadBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
               VkBuffer &outBuffer, VmaAllocation &outAllocation);

  // Device & Swapchain Setup
  bool isDeviceSuitable();
  void cleanupSwapchain();
  std::expected<void, std::string> pickPhysicalDevice();
  std::expected<void, std::string> createLogicalDevice();
  std::expected<void, std::string> setupDebugMessenger();
  std::expected<void, std::string> createSurface();
  std::expected<void, std::string> createSwapChain();
  std::expected<void, std::string> createGraphicsPipeline();
  std::expected<void, std::string> createDepthResources();
  std::expected<void, std::string> createFramebuffers();
  std::expected<void, std::string> createCommandInfrastructure();
  std::expected<void, std::string> createAllocator();
  std::expected<void, std::string> createSyncObjects();
  std::expected<void, std::string> createDescriptorSetLayout();
  std::expected<void, std::string> createUboResources();

  // Shader compilation helper
  std::expected<VkShaderModule, std::string>
  createShaderModule(const std::string &shaderPath);

  // Swapchain helper formats
  SwapChainSupportDetails querySwapChainSupportDetails(VkPhysicalDevice device);
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
  std::expected<void, std::string> createSwapchainImageViews();
  std::expected<void, std::string> createRenderPass();

private:
  struct FrameUBO {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    void *mappedData{nullptr};
  };

  // Viewport states
  OffscreenRenderTarget m_viewportTarget;
  VkSampler m_viewportSampler = VK_NULL_HANDLE;

  // Active Scene Meshes
  MeshHandle m_next_mesh_handle{1};
  std::unordered_map<MeshHandle, VulkanMeshBackend> m_meshes;

  // Vulkan Descriptor management
  VkDescriptorSetLayout m_globalDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
  std::vector<FrameUBO> m_frameUboBuffers;
  std::vector<VkDescriptorSet> m_globalDescriptorSets;

  // Core Hardware Contexts
  VkInstance m_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VkQueue m_graphics_queue = VK_NULL_HANDLE;
  VkQueue m_present_queue = VK_NULL_HANDLE;

  uint32_t m_graphicsQueueFamilyIndex = 0xFFFFFFFF;
  uint32_t m_presentQueueFamilyIndex = 0xFFFFFFFF;

  std::vector<const char *> m_deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  // Swapchain resource caches
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;
  VkFormat m_swapChainImageFormat;
  VkExtent2D m_swapChainExtent;

  // Rendering execution pipelines
  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

  // Synchronization structures
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
  size_t m_currentFrame = 0;
  VkCommandPool m_commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> m_commandBuffers;

  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;

  // Hardware Swapchain Depth Contexts
  VkFormat m_depthFormat;
  VkImage m_depthImage = VK_NULL_HANDLE;
  VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
  VkImageView m_depthImageView = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> m_swapChainFramebuffers;

  bool m_validationEnabled = false;
  RendererConfig m_RendererConfig;
};

} // namespace ob
