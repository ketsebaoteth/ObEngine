#include "imgui_impl_vulkan.h"
#include "log/log.hpp"
#include "rhi/renderer.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "volk.h"
#include <GLFW/glfw3.h>
#include <expected>
#include <set>
#include <string>
#include <vector>

namespace ob {

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  OB_CORE_ERROR("[vulkan validation]: %s", pCallbackData->pMessage);
  return VK_FALSE;
}

std::expected<void, std::string> VulkanRenderer::setupDebugMessenger() {

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  return {};
}

std::expected<void, std::string> VulkanRenderer::createSurface() {
  if (!m_RendererConfig.surface_creator) {
    return std::unexpected(
        "Cannot create surface: surface_creator callback is null");
  }
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  int result = m_RendererConfig.surface_creator(
      static_cast<void *>(m_instance), nullptr, static_cast<void *>(&surface));

  if (result != VK_SUCCESS) { // VK_SUCCESS is 0
    return std::unexpected("Failed to create window surface via callback");
  }

  m_surface = surface;
  OB_CORE_INFO("Vulkan surface created successfully via GLFW.");
  return {};
}

std::expected<void, std::string> VulkanRenderer::createAllocator() {
  VmaVulkanFunctions vmaFunctions{};

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.instance = m_instance;
  allocatorInfo.physicalDevice = m_physicalDevice;
  allocatorInfo.device = m_device;
  allocatorInfo.pVulkanFunctions = nullptr;

  if (vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vmaFunctions) !=
      VK_SUCCESS) {
    return std::unexpected("Vulkan Initialization Error: Volk failed to map "
                           "function pointers to VMA dispatch table.");
  }

  allocatorInfo.pVulkanFunctions = &vmaFunctions;

  if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
    return std::unexpected("Vulkan Initialization Error: Failed to instantiate "
                           "VMA Memory Allocator context.");
  }

  OB_CORE_INFO(
      "Vulkan Memory Allocator (VMA) engine successfully initialized.");
  return {};
}

VkPhysicalDevice selectBestGPU(const std::vector<VkPhysicalDevice> &devices,
                               VkSurfaceKHR surface, uint32_t &outGraphicsIdx,
                               uint32_t &outPresentIdx) {
  VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
  uint32_t highestScore = 0;

  for (const auto &device : devices) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);

    if (!features.geometryShader) {
      OB_CORE_TRACE("  - Skipped [{}]: Missing geometry shader support.",
                    properties.deviceName);
      continue;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());

    uint32_t candidateGraphicsIdx = 0xFFFFFFFF;
    uint32_t candidatePresentIdx = 0xFFFFFFFF;

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        candidateGraphicsIdx = i;
      }

      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
      if (presentSupport) {
        candidatePresentIdx = i;
      }

      if (candidateGraphicsIdx != 0xFFFFFFFF &&
          candidatePresentIdx != 0xFFFFFFFF) {
        break;
      }
    }

    if (candidateGraphicsIdx == 0xFFFFFFFF ||
        candidatePresentIdx == 0xFFFFFFFF) {
      OB_CORE_TRACE(
          "  - Skipped [{}]: Missing required Graphics or Present queues.",
          properties.deviceName);
      continue;
    }

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         nullptr);

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                              &presentModeCount, nullptr);

    if (formatCount == 0 || presentModeCount == 0) {
      OB_CORE_TRACE(
          "  - Skipped [{}]: Surface/Swapchain capabilities are incompatible.",
          properties.deviceName);
      continue;
    }

    uint32_t currentScore = 0;

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      currentScore += 10000;
    } else if (properties.deviceType ==
               VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      currentScore += 1000;
    }

    currentScore += properties.limits.maxImageDimension2D;

    OB_CORE_TRACE("  - Evaluated [{}]: Score = {}", properties.deviceName,
                  currentScore);

    if (currentScore > highestScore) {
      highestScore = currentScore;
      bestDevice = device;
      outGraphicsIdx = candidateGraphicsIdx;
      outPresentIdx = candidatePresentIdx;
    }
  }

  if (bestDevice != VK_NULL_HANDLE) {
    VkPhysicalDeviceProperties bestProperties;
    vkGetPhysicalDeviceProperties(bestDevice, &bestProperties);
    OB_CORE_INFO("Selected Hardware Device Target: [{}] (Graphics Queue: {}, "
                 "Present Queue: {})",
                 bestProperties.deviceName, outGraphicsIdx, outPresentIdx);
  } else {
    OB_CORE_ERROR("Hardware Selection Error: No suitable Vulkan GPU found.");
  }

  return bestDevice;
}
std::expected<void, std::string> VulkanRenderer::pickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    return std::unexpected(
        "Vulkan failed to enumerate any valid physical devices. Ensure your "
        "graphics drivers are installed correctly");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
  m_physicalDevice =
      selectBestGPU(devices, m_surface, m_graphicsQueueFamilyIndex,
                    m_presentQueueFamilyIndex);
  if (m_physicalDevice == VK_NULL_HANDLE) {
    return std::unexpected("Could not find a suitable physical device.");
  }
  const std::vector<VkFormat> depthCandidates = {VK_FORMAT_D32_SFLOAT,
                                                 VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                 VK_FORMAT_D24_UNORM_S8_UINT};

  m_depthFormat = VK_FORMAT_UNDEFINED;
  for (VkFormat format : depthCandidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

    if ((props.optimalTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ==
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      m_depthFormat = format;
      break;
    }
  }

  if (m_depthFormat == VK_FORMAT_UNDEFINED) {
    return std::unexpected("Hardware Selection Error: Selected GPU does not "
                           "support a standard 3D depth format.");
  }
  return {};
};

std::expected<void, std::string> VulkanRenderer::createLogicalDevice() {
  std::set<uint32_t> uniqueQueueFamilies = {m_graphicsQueueFamilyIndex};
  uniqueQueueFamilies.insert(m_presentQueueFamilyIndex);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  float queuePriority = 1.0f;

  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo q{};
    q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q.queueFamilyIndex = queueFamily;
    q.queueCount = 1;
    q.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(q);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = VK_TRUE;
  deviceFeatures.fillModeNonSolid = VK_TRUE;
  deviceFeatures.wideLines = VK_TRUE;
  deviceFeatures.geometryShader = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(m_deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

  if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) !=
      VK_SUCCESS) {
    return std::unexpected("Unable to create logical device");
  }

  volkLoadDevice(m_device);

  vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphics_queue);
  vkGetDeviceQueue(m_device, m_presentQueueFamilyIndex, 0, &m_present_queue);
  OB_CORE_INFO(
      "Vulkan logical device and execution queues initialized successfully.");
  return {};
};
std::expected<void, std::string> VulkanRenderer::createCommandInfrastructure() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

  if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) !=
      VK_SUCCESS) {
    return std::unexpected("Vulkan Infrastructure Error: Failed to instantiate "
                           "graphics command allocation pool.");
  }
  m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) !=
      VK_SUCCESS) {
    return std::unexpected("Vulkan Infrastructure Error: Failed to allocate "
                           "concurrent primary execution command buffers.");
  }

  OB_CORE_INFO("Vulkan execution pipeline command pools and buffers "
               "successfully initialized.");
  return {};
}
std::expected<void, std::string>
VulkanRenderer::createSyncObjects() { // 1. Maintain Frame tracking constraints
                                      // for host fences and acquisition
  m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  m_renderFinishedSemaphores.resize(m_swapchainImageViews.size());

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                          &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) !=
            VK_SUCCESS) {
      return std::unexpected(
          "Vulkan Infrastructure Error: Failed to instantiate "
          "frame sync assets.");
    }
  }

  for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
    if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                          &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
      return std::unexpected(
          "Vulkan Infrastructure Error: Failed to instantiate "
          "swapchain signal assets.");
    }
  }

  OB_CORE_INFO("Vulkan host-to-device asynchronous frame synchronization layer "
               "fully locked and loaded.");
  return {};
}
void VulkanRenderer::register_imgui_viewport_texture() {
  if (m_viewportTarget.imguiDescriptorSet != VK_NULL_HANDLE) {
    ImGui_ImplVulkan_RemoveTexture(m_viewportTarget.imguiDescriptorSet);
  }

  m_viewportTarget.imguiDescriptorSet =
      ImGui_ImplVulkan_AddTexture(m_viewportSampler, m_viewportTarget.colorView,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

VulkanContext VulkanRenderer::get_vulkan_context() const {
  VulkanContext context;
  context.device = m_device;
  context.physicalDevice = m_physicalDevice;
  context.graphicsQueue = m_graphics_queue;
  context.instance = m_instance;
  context.renderPass = m_renderPass;
  context.commandBuffer = m_commandBuffers[m_currentFrame];
  return context;
}

std::expected<void, std::string>
VulkanRenderer::init(const RendererConfig &rendererConfig) {
  m_RendererConfig = rendererConfig;
  auto res = volkInitialize();
  if (res != VK_SUCCESS) {
    return std::unexpected("Failed to initialize Volk - Vulkan not available");
  }

  VkApplicationInfo appinfo{};
  appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appinfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appinfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appinfo.apiVersion = VK_API_VERSION_1_3;
  appinfo.pEngineName = "ob";
  appinfo.pApplicationName = "obEngine";

  std::vector<const char *> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};

  extensions.insert(extensions.end(), rendererConfig.vulkanExtensions.begin(),
                    rendererConfig.vulkanExtensions.end());

  std::vector<const char *> validationLayers;
  if (rendererConfig.validation) {
    validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  }

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appinfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();
  createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
  createInfo.ppEnabledLayerNames = validationLayers.data();

  VkResult createInstanceRes =
      vkCreateInstance(&createInfo, nullptr, &m_instance);
  if (createInstanceRes != VK_SUCCESS) {
    return std::unexpected("vkCreateInstance failed");
  }
  volkLoadInstance(m_instance);

  if (rendererConfig.validation) {
    auto res = setupDebugMessenger();
    if (!res) {
      OB_CORE_ERROR("%s", res.error());
    }
  }

  return createSurface()
      .and_then([&]() { return pickPhysicalDevice(); })
      .and_then([&]() { return createLogicalDevice(); })
      .and_then([&]() { return createAllocator(); })
      .and_then([&]() { return createDescriptorSetLayout(); })
      .and_then([&]() { return createSwapChain(); })
      .and_then([&]() { return createSwapchainImageViews(); })
      .and_then([&]() { return createRenderPass(); })
      .and_then([&]() { return createGraphicsPipeline(); })
      .and_then([&]() { return createDepthResources(); })
      .and_then([&]() { return createFramebuffers(); })
      .and_then([&]() { return createUboResources(); })
      .and_then([&]() { return createCommandInfrastructure(); })
      .and_then([&]() { return createSyncObjects(); })
      .and_then([&]() {
        return createOffscreenRenderTarget(rendererConfig.width,
                                           rendererConfig.height);
      });
}

VulkanRenderer::~VulkanRenderer() { shutdown(); }

void VulkanRenderer::shutdown() {
  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
  }

  // 1. Destroy frame-in-flight sync assets safely
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
    }
    if (m_inFlightFences[i] != VK_NULL_HANDLE) {
      vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
  }

  // FIX 1: Destroy render finished semaphores scaled to the actual allocated
  // array size
  for (VkSemaphore semaphore : m_renderFinishedSemaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_device, semaphore, nullptr);
    }
  }
  m_renderFinishedSemaphores.clear();

  if (m_commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;
  }

  for (VkFramebuffer framebuffer : m_swapChainFramebuffers) {
    if (framebuffer != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
  }
  m_swapChainFramebuffers.clear();

  if (m_graphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    m_graphicsPipeline = VK_NULL_HANDLE;
  }

  if (m_pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    m_pipelineLayout = VK_NULL_HANDLE;
  }

  if (m_renderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;
  }

  if (m_depthImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    m_depthImageView = VK_NULL_HANDLE;
  }

  if (m_depthImage != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, m_depthImage, m_depthAllocation);
    m_depthImage = VK_NULL_HANDLE;
    m_depthAllocation = VK_NULL_HANDLE;
  }

  // FIX 2: Explicitly matching your uppercase 'C' vector tracking container
  for (VkImageView imageView : m_swapchainImageViews) {
    if (imageView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_device, imageView, nullptr);
    }
  }
  m_swapchainImageViews.clear();
  m_swapchainImages.clear();

  if (m_swapchain != VK_NULL_HANDLE) { // Check spelling layout (capital C)
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }

  // VMA allocator MUST die before the parent VkDevice handle is destroyed
  if (m_allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(m_allocator);
    m_allocator = VK_NULL_HANDLE;
  }

  if (m_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  if (m_debugMessenger != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    m_debugMessenger = VK_NULL_HANDLE;
  }

  if (m_surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }

  if (m_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }
}
void VulkanRenderer::waitDeviceIdle() {
  if (m_device != VK_NULL_HANDLE) {
    VkResult result = vkDeviceWaitIdle(m_device);
    if (result != VK_SUCCESS) {
      OB_CORE_ERROR("Vulkan Sync Error: Failed to stall device graphics queue "
                    "processing loops safely.");
    }
  }
}

void VulkanRenderer::resize(uint32_t w, uint32_t h) {
  if (w == 0 || h == 0) {
    return;
  }
  waitDeviceIdle();
  cleanupSwapchain();
  m_RendererConfig.width = w;
  m_RendererConfig.height = h;
  auto resSC = createSwapChain();
  if (!resSC) {
    OB_CORE_ERROR("Swapchain Recreation Failure: {}", resSC.error());
    return;
  }

  auto resIV = createSwapchainImageViews();
  if (!resIV) {
    OB_CORE_ERROR("Swapchain Image Views Recreation Failure: {}",
                  resIV.error());
    return;
  }

  auto resDR = createDepthResources();
  if (!resDR) {
    OB_CORE_ERROR("Depth Resources Recreation Failure: {}", resDR.error());
    return;
  }

  auto resFB = createFramebuffers();
  if (!resFB) {
    OB_CORE_ERROR("Framebuffer Generation Failure: {}", resFB.error());
    return;
  }
}
} // namespace ob
