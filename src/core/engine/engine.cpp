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

  // Set exactly at the center (0, 0, 0)
  m_active_scene->registry().emplace<TransformComponent>(
      cubeEntity, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(1.0f, 1.0f, 1.0f));
  m_active_scene->registry().emplace<MeshComponent>(cubeEntity, cube);

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

void Engine::update(float deltaTime) {
  auto view =
      m_active_scene->registry().view<TransformComponent, MeshComponent>();
  for (auto entity : view) {
    auto &transform = view.get<TransformComponent>(entity);
    transform.rotation.z += 0.5f * deltaTime;
  }
}

void Engine::render() {
  m_layer_manager->render();

  uint32_t width = m_window_manager->get_window_impl()->get_width();
  uint32_t height = m_window_manager->get_window_impl()->get_height();

  m_active_scene->draw(m_renderer.get(), width, height);
}

} // namespace ob
