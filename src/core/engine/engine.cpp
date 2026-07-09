// In engine.cpp
#include "engine/engine.hpp"
#include "log/log.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "scene/Components.hpp"
#include "scene/Scene.hpp"
#include <memory>

namespace ob {

Engine::Engine() : isRunning(false), frameCount(0) {
  m_active_renderer_type = RendererType::VULKAN;
  ob::Log::init();
}

Engine::~Engine() {
  if (m_renderer) {
    m_renderer->waitDeviceIdle();
  }
  m_layer_manager.reset();
  m_active_scene.reset();
  if (m_renderer) {
    m_renderer->shutdown();
  }
  OB_CORE_INFO("Engine structures successfully broken down via RAII.");
}

std::expected<void, std::string> Engine::init() {
  m_window_manager = std::make_unique<WindowManager>();
  auto windowRes = m_window_manager->init();
  if (!windowRes) {
    return std::unexpected("Window System Initialization Failed: " +
                           windowRes.error());
  }

  m_event_manager = std::make_unique<EventManager>();

  OB_CORE_INFO("Initializing Renderer context...");
  m_renderer = std::make_unique<VulkanRenderer>();
  auto rendererRes = m_renderer->init(
      m_window_manager->get_renderer_config(m_active_renderer_type));
  if (!rendererRes) {
    return std::unexpected("Renderer Initialization Failed: " +
                           rendererRes.error());
  }

  m_active_scene = std::make_unique<Scene>(m_renderer.get());
  m_layer_manager = std::make_unique<LayerManager>(
      m_window_manager->get_window_impl(), m_renderer.get(),
      m_event_manager.get(), m_active_scene.get());

  m_asset_manager = std::make_unique<AssetManager>(m_renderer.get());

  MeshHandle cube = m_asset_manager->getMesh("cube");
  auto cubeEntity = m_active_scene->createEntity();

  m_active_scene->registry().emplace<TagComponent>(cubeEntity, "defaultCube");
  m_active_scene->registry().emplace<TransformComponent>(
      cubeEntity, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(1.0f, 1.0f, 1.0f));
  m_active_scene->registry().emplace<MeshComponent>(cubeEntity, cube);

  MeshHandle cube2 = m_asset_manager->getMesh("cube");
  auto cubeEntity2 = m_active_scene->createEntity();

  m_active_scene->registry().emplace<TagComponent>(cubeEntity2, "defaultCube2");
  m_active_scene->registry().emplace<TransformComponent>(
      cubeEntity2, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(1.0f, 1.0f, 1.0f));
  m_active_scene->registry().emplace<MeshComponent>(cubeEntity2, cube2);

  auto blueLight = m_active_scene->createEntity();
  m_active_scene->registry().emplace<TagComponent>(blueLight, "bluelight");
  m_active_scene->registry().emplace<TransformComponent>(
      blueLight, glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
  m_active_scene->registry().emplace<PointLightComponent>(
      blueLight, glm::vec3(0.05f, 0.05f, 1.0f), 5.0f, 6.0f);

  auto &mat =
      m_active_scene->registry().emplace<PBRMaterialComponent>(cubeEntity);
  mat.name = "Gold Cube";
  mat.scalarParameters["metallic"] = 1.0f;
  mat.scalarParameters["roughness"] = 0.5f;
  mat.vectorParameters["baseColor"] = glm::vec4(1.0f, 0.78f, 0.0f, 1.0f);
  //
  auto &mat2 =
      m_active_scene->registry().emplace<PBRMaterialComponent>(cubeEntity2);
  mat2.name = "Gold Cube";
  mat2.scalarParameters["metallic"] = 1.0f;  // Make it a pure shiny gold metal!
  mat2.scalarParameters["roughness"] = 0.5f; // High gloss!
  mat2.vectorParameters["baseColor"] = glm::vec4(1.0f, 0.78f, 0.0f, 1.0f);

  m_last_frame_time = std::chrono::steady_clock::now();
  OB_CORE_INFO("Engine core and graphics subsystems successfully loaded.");
  return {};
}

void Engine::run() {
  isRunning = true;
  auto window_impl = m_window_manager->get_window_impl();
  while (isRunning) {
    std::vector<Event> events = window_impl->get_events();
    m_event_manager->update(events);

    for (const auto &event : events) {
      if (event.type == EventType::WindowResize) {
        m_renderer->resize(event.window.width, event.window.height);
      }
    }

    if (window_impl->should_close()) {
      isRunning = false;
    }

    auto current_time = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = current_time - m_last_frame_time;
    float deltaTime = elapsed.count();
    m_last_frame_time = current_time;

    if (deltaTime > 0.1f) {
      deltaTime = 0.1f;
    }

    m_layer_manager->update(deltaTime);
    update(deltaTime);
    render();

    frameCount++;
  }
}

void Engine::update(float deltaTime) {}

void Engine::render() {
  m_layer_manager->render();

  uint32_t width = m_renderer->getViewportWidth();
  uint32_t height = m_renderer->getViewportHeight();

  m_active_scene->draw(m_renderer.get(), width, height);
}

} // namespace ob
