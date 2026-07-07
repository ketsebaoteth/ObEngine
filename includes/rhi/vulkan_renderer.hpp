#pragma once
#include "renderer.hpp"
#include <expected>
#include <scene/Components.hpp>
#include <string>
// clang-format off
#include <volk.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"
// clang-format on

namespace ob {
struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capablities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct GlobalUboPayload {
  glm::mat4 view;
  glm::mat4 proj;
};

class VulkanRenderer : public IRenderer {
public:
  VulkanRenderer() = default;
  ~VulkanRenderer() override;

  [[nodiscard]] MeshHandle
  uploadMesh(std::span<const Vertex> vertices,
             std::span<const uint32_t> indices) override;
  void destroyMesh(MeshHandle handle) override;

  std::expected<void, std::string> init(const NativeWindowHandle &windowHandle,
                                        const RendererConfig &config) override;
  void shutdown() override;
  void resize(uint32_t w, uint32_t h) override;
  void present(std::span<const RenderItem> renderQueue, const glm::mat4 &view,
               const glm::mat4 &proj) override;

  VmaAllocator getAllocator() { return m_allocator; }
  VkDevice getDevice() const { return m_device; }

  struct VulkanMeshBackend {
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VmaAllocation vertexAllocation{VK_NULL_HANDLE};
    uint32_t vertexCount{0};

    VkBuffer indexBuffer{VK_NULL_HANDLE};
    VmaAllocation indexAllocation{VK_NULL_HANDLE};
    uint32_t indexCount{0};
  };

  void waitDeviceIdle() override;

private:
  std::expected<void, std::string>
  uploadBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
               VkBuffer &outBuffer, VmaAllocation &outAllocation);

  MeshHandle m_next_mesh_handle{1};
  std::unordered_map<MeshHandle, VulkanMeshBackend> m_meshes;

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

  // graphics pipeline related
  std::expected<VkShaderModule, std::string>
  createShaderModule(const std::string &shaderPath);

  // swapchain related
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

  VkDescriptorSetLayout m_globalDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
  std::vector<FrameUBO> m_frameUboBuffers;
  std::vector<VkDescriptorSet> m_globalDescriptorSets;

  std::vector<const char *> m_deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  VkInstance m_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VkQueue m_graphics_queue;
  VkQueue m_present_queue;
  uint32_t m_graphicsQueueFamilyIndex = 0xFFFFFFFF;
  uint32_t m_presentQueueFamilyIndex = 0xFFFFFFFF;
  VkPipelineLayout m_pipelineLayout;
  VkPipeline m_graphicsPipeline;
  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;
  VkFormat m_swapChainImageFormat;
  VkFormat m_depthFormat;
  VkExtent2D m_swapChainExtent;
  VkSwapchainKHR m_swapchain;
  VkRenderPass m_renderPass;
  VmaAllocator m_allocator = VK_NULL_HANDLE;
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
  size_t m_currentFrame = 0;
  VkCommandPool m_commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> m_commandBuffers;

  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;

  // Hardware Depth Storage Contexts
  VkImage m_depthImage = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> m_swapChainFramebuffers;
  VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
  VkImageView m_depthImageView = VK_NULL_HANDLE;
  bool m_validationEnabled = false;
  NativeWindowHandle m_nativeWindowHandle;
  RendererConfig m_RendererConfig;
};
} // namespace ob
