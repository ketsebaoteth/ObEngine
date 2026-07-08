#include "ui/imgui_layer.hpp"
#include "log/log.hpp"
#include "rhi/renderer.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "windowing/window.hpp"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace ob {

ImGuiLayer::ImGuiLayer(IWindow *window, IRenderer *renderer, RendererType type)
    : m_window(window), m_renderer(renderer), m_renderer_type(type) {}

void ImGuiLayer::on_attach() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Keep docking alive

  ImGui::StyleColorsDark();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = 1000 * std::size(poolSizes);
  poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
  poolInfo.pPoolSizes = poolSizes;
  // TODO: this will be undefined if renderer type is not vulkan
  VulkanRenderer *renderer = nullptr;
  if (m_renderer_type == RendererType::VULKAN) {
    renderer = static_cast<VulkanRenderer *>(m_renderer);
  }
  VkDevice device = renderer->getDevice();
  if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_imguiPool) !=
      VK_SUCCESS) {
    OB_CORE_ERROR("UI Error: Failed to generate isolated ImGui descriptor "
                  "allocation block.");
    return;
  }

  GLFWwindow *windowPtr =
      static_cast<GLFWwindow *>(m_window->get_native_window_ptr());
  ImGui_ImplGlfw_InitForVulkan(windowPtr, true);

  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.ApiVersion = VK_API_VERSION_1_3;
  initInfo.Instance = renderer->getInstance();
  initInfo.PhysicalDevice = renderer->getPhysicalDevice();
  initInfo.Device = device;
  initInfo.QueueFamily =
      0; // Ensure this matches your graphics queue family index
  initInfo.Queue = renderer->getGraphicsQueue();
  initInfo.DescriptorPool = m_imguiPool;
  initInfo.MinImageCount = 2;
  initInfo.ImageCount = 2;
  initInfo.PipelineCache = VK_NULL_HANDLE;
  initInfo.UseDynamicRendering = false;

  initInfo.PipelineInfoMain.RenderPass = renderer->getRenderPass();
  initInfo.PipelineInfoMain.Subpass = 0;
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  initInfo.Allocator = nullptr;
  initInfo.CheckVkResultFn = [](VkResult err) {
    if (err == VK_SUCCESS)
      return;
    OB_CORE_ERROR("Internal ImGui Vulkan Backend Failure: VkResult {}",
                  static_cast<int>(err));
  };

  if (!ImGui_ImplVulkan_Init(&initInfo)) {
    OB_CORE_ERROR("UI Framework Error: Failed to initialize ImGui Vulkan "
                  "hardware attachments.");
    return;
  }

  m_renderer->register_imgui_viewport_texture();
}

void ImGuiLayer::on_detach() {
  VulkanRenderer *renderer = nullptr;
  if (m_renderer_type == RendererType::VULKAN) {
    renderer = static_cast<VulkanRenderer *>(m_renderer);
  }
  VkDevice device = renderer->getDevice();
  vkDeviceWaitIdle(
      device); // Block to make sure nothing is drawing while tearing down

  if (m_viewportDescriptorSet != VK_NULL_HANDLE) {
    ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptorSet);
    m_viewportDescriptorSet = VK_NULL_HANDLE;
  }

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (m_imguiPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_imguiPool, nullptr);
    m_imguiPool = VK_NULL_HANDLE;
  }
}

void ImGuiLayer::begin_ui_frame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::on_ui_render() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("ObliqueWorkspaceDockspace", nullptr, windowFlags);
  ImGui::PopStyleVar(3);

  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspaceId = ImGui::GetID("MyEngineDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
  }

  if (m_viewportImageView != m_lastTrackedImageView) {
    if (m_viewportDescriptorSet != VK_NULL_HANDLE) {
      ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptorSet);
    }
    m_viewportDescriptorSet =
        ImGui_ImplVulkan_AddTexture(m_viewportSampler, m_viewportImageView,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_lastTrackedImageView = m_viewportImageView;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  ImGui::Begin("Scene Viewport");

  ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
  ImGui::Image(
      reinterpret_cast<ImTextureID>(m_renderer->get_viewport_texture_id()),
      viewportPanelSize);

  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::End();
}

} // namespace ob
