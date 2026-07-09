#include "ui/imgui_layer.hpp"
#include "log/log.hpp"
#include "rhi/renderer.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "utils/io.hpp"
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
  io.FontGlobalScale = 1.2f;

  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(1.4f);

  // Modern, sleek flat roundings
  style.WindowRounding = 4.0f;
  style.ChildRounding = 2.0f;
  style.FrameRounding = 3.0f;
  style.PopupRounding = 3.0f;
  style.ScrollbarRounding = 9.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f; // Subtle border around input fields
  style.PopupBorderSize = 1.0f;

  // --- OBSIDIAN BLACK-OUT PALETTE (RGB 23, 23, 23 Base) ---
  ImVec4 *colors = style.Colors;

  // Base RGB(23,23,23) normalized is exactly 0.09f
  const ImVec4 baseBlack = ImVec4(0.015f, 0.015f, 0.015f, 1.00f);
  // Slightly lighter colors for contrast layers (RGB 28 to 35)
  const ImVec4 darkContrast = ImVec4(0.022f, 0.022f, 0.022f, 1.00f);
  const ImVec4 midContrast = ImVec4(0.035f, 0.035f, 0.035f, 1.00f);
  const ImVec4 grayHighlight = ImVec4(0.055f, 0.055f, 0.055f, 1.00f); // Texts
  std::string fontPath = io::ResolveAssetPath("font/Inter_18pt-Regular.ttf");
  ImFont *font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f);
  if (font == nullptr) {
    OB_CORE_WARN("UI Warning: Failed to load custom font from [{}]. Falling "
                 "back to default.",
                 fontPath);
  } else {
    io.FontDefault = font;
  }

  colors[ImGuiCol_Text] =
      ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // Clean muted white
  colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

  // Background layers
  colors[ImGuiCol_WindowBg] = baseBlack;
  colors[ImGuiCol_ChildBg] = baseBlack;
  colors[ImGuiCol_PopupBg] = baseBlack;

  // Header and Title blocks
  colors[ImGuiCol_TitleBg] = baseBlack;
  colors[ImGuiCol_TitleBgActive] = baseBlack;
  colors[ImGuiCol_TitleBgCollapsed] = baseBlack;
  colors[ImGuiCol_MenuBarBg] = baseBlack;

  // Subtle borders to separate panels without breaking the black-out theme
  colors[ImGuiCol_Border] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  // Interactive Widgets (Sliders, inputs)
  colors[ImGuiCol_FrameBg] = darkContrast;
  colors[ImGuiCol_FrameBgHovered] = midContrast;
  colors[ImGuiCol_FrameBgActive] = grayHighlight;

  // Clickable Buttons
  colors[ImGuiCol_Button] = darkContrast;
  colors[ImGuiCol_ButtonHovered] = midContrast;
  colors[ImGuiCol_ButtonActive] = grayHighlight;

  // Tree nodes, list selects
  colors[ImGuiCol_Header] = darkContrast;
  colors[ImGuiCol_HeaderHovered] = midContrast;
  colors[ImGuiCol_HeaderActive] = grayHighlight;

  // Splitter/Drag bars
  colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

  // Tabs (Dockspace headers)
  colors[ImGuiCol_Tab] = baseBlack;
  colors[ImGuiCol_TabHovered] = midContrast;
  colors[ImGuiCol_TabActive] = darkContrast; // Sleek flat tab fusion
  colors[ImGuiCol_TabUnfocused] = baseBlack;
  colors[ImGuiCol_TabUnfocusedActive] = darkContrast;
  // Checkboxes
  colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

  // Scrollbars
  colors[ImGuiCol_ScrollbarBg] = baseBlack;
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

  // Docking guidelines (keep them extremely minimal)
  colors[ImGuiCol_DockingPreview] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
  colors[ImGuiCol_DockingEmptyBg] = baseBlack;

  // Window resize handle
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

  // Graphs and Plots (in case you add frame monitors later)
  colors[ImGuiCol_PlotLines] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

  // Text selection background
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.35f, 0.35f, 0.35f);

  // Drag-and-drop targets
  colors[ImGuiCol_DragDropTarget] = ImVec4(0.80f, 0.80f, 0.80f, 0.90f);

  // Keyboard navigation outlines
  colors[ImGuiCol_NavHighlight] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.30f, 0.30f, 0.30f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
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
