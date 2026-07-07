#include "engine/engine.hpp"
#include "log/log.hpp"
#include "rhi/vulkan_renderer.hpp"
#include "scene/Scene.hpp"
#include "windowing/glfw_window.hpp"
#include "windowing/window.hpp"
#include <memory>

namespace ob {

Engine::Engine() : isRunning(false), frameCount(0) { ob::Log::init(); }

Engine::~Engine() {
  if (m_renderer) {
    m_renderer->waitDeviceIdle();
  }
  if (m_active_scene) {
    auto view = m_active_scene->registry().view<MeshComponent>();
    for (auto entity : view) {
      auto &mesh = view.get<MeshComponent>(entity);
      if (mesh.handle != 0) {
        m_renderer->destroyMesh(mesh.handle);
        mesh.handle = 0;
      }
    }
  }
  if (m_renderer) {
    m_renderer->shutdown();
    m_renderer.reset();
  }
  m_engine_window->shutdown();
  OB_CORE_INFO("Engine structures successfully broken down via RAII.");
}

std::expected<void, std::string> Engine::init() {
  WindowConfig config{};
  config.height = 900;
  config.width = 1200;
  config.title = "Oblique Engine";

  m_engine_window = std::make_unique<GlfwWindow>();

  auto windowRes = m_engine_window->init(config);
  if (!windowRes) {
    return std::unexpected("Window System Initialization Failed: " +
                           windowRes.error());
  }

  ob::NativeWindowHandle nativeHandle = m_engine_window->get_native_handle();

  ob::RendererConfig rendererConfig{};
  rendererConfig.width = config.width;
  rendererConfig.height = config.height;
  rendererConfig.vsync = true;
#ifdef NDEBUG
  rendererConfig.validation = false;
#else
  rendererConfig.validation = true;
#endif

  OB_CORE_INFO("Initializing Renderer context...");
  m_renderer = std::make_unique<VulkanRenderer>();

  auto rendererRes = m_renderer->init(nativeHandle, rendererConfig);
  if (!rendererRes) {
    return std::unexpected("Renderer Initialization Failed: " +
                           rendererRes.error());
  }
  m_active_scene = std::make_unique<Scene>();

  std::vector<Vertex> triVertices = {
      {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
      {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};

  std::vector<uint32_t> triIndices = {0, 1, 2};

  MeshHandle triHandle = m_renderer->uploadMesh(triVertices, triIndices);
  if (triHandle == 0) {
    return std::unexpected("Failed to populate 2D triangle assets.");
  }

  auto triEntity = m_active_scene->createEntity();
  m_active_scene->registry().emplace<TagComponent>(triEntity, "2D Triangle");

  m_active_scene->registry().emplace<TransformComponent>(
      triEntity, glm::vec3(0.0f, 0.0f, -2.0f), // Translation
      glm::vec3(0.0f, 0.0f, 0.0f),             // Rotation
      glm::vec3(1.0f, 1.0f, 1.0f)              // Scale
  );

  m_active_scene->registry().emplace<MeshComponent>(triEntity, triHandle);

  m_last_frame_time = std::chrono::steady_clock::now();
  OB_CORE_INFO("Engine core and graphics subsystems successfully loaded.");
  return {};
}

void Engine::run() {
  isRunning = true;

  while (isRunning) {
    std::vector<Event> events = m_engine_window->get_events();
    for (const auto &event : events) {
      if (event.type == EventType::WindowResize) {
        m_renderer->resize(event.window.width, event.window.height);
      }

      else if (event.type == EventType::KeyPressed) {
        if (event.key.code >= 0 && event.key.code < 512) {
          m_keys_pressed[event.key.code] = true;
        }
      } else if (event.type == EventType::KeyReleased) {
        if (event.key.code >= 0 && event.key.code < 512) {
          m_keys_pressed[event.key.code] = false;
        }
      }

      else if (event.type == EventType::MouseButtonPressed) {
        if (event.mouse_button.button == 0) {
          m_right_mouse_held = true;
        }
      } else if (event.type == EventType::MouseButtonReleased) {
        if (event.mouse_button.button == 0) {
          m_right_mouse_held = false;
          m_first_mouse_move = true;
        }
      }

      else if (event.type == EventType::MouseMoved) {
        if (m_right_mouse_held) {
          if (m_first_mouse_move) {
            m_last_mouse_x = event.mouse_moved.x;
            m_last_mouse_y = event.mouse_moved.y;
            m_first_mouse_move = false;
          }

          double delta_x = event.mouse_moved.x - m_last_mouse_x;
          double delta_y = m_last_mouse_y - event.mouse_moved.y;

          m_last_mouse_x = event.mouse_moved.x;
          m_last_mouse_y = event.mouse_moved.y;

          float mouseSensitivity = 0.1f;
          m_editor_camera.processMouseMovement(
              static_cast<float>(delta_x) * mouseSensitivity,
              static_cast<float>(delta_y) * mouseSensitivity);
        }
      }
    }
    if (m_engine_window->should_close()) {
      isRunning = false;
      break;
    }

    auto current_time = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = current_time - m_last_frame_time;
    float deltaTime = elapsed.count();
    m_last_frame_time = current_time;

    if (deltaTime > 0.1f)
      deltaTime = 0.1f;

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

  float cameraSpeed = 5.0f * deltaTime;

  if (m_keys_pressed[87 /* GLFW_KEY_W */])
    m_editor_camera.moveForward(cameraSpeed);
  if (m_keys_pressed[83 /* GLFW_KEY_S */])
    m_editor_camera.moveForward(-cameraSpeed);

  if (m_keys_pressed[65 /* GLFW_KEY_A */])
    m_editor_camera.moveRight(-cameraSpeed);
  if (m_keys_pressed[68 /* GLFW_KEY_D */])
    m_editor_camera.moveRight(cameraSpeed);

  if (m_keys_pressed[81 /* GLFW_KEY_Q */])
    m_editor_camera.moveUp(-cameraSpeed);
  if (m_keys_pressed[69 /* GLFW_KEY_E */])
    m_editor_camera.moveUp(cameraSpeed);
}

void Engine::render() {
  std::vector<RenderItem> renderQueue;
  glm::mat4 viewMatrix(1.0f);
  glm::mat4 projectionMatrix(1.0f);

  uint32_t width = m_engine_window->get_width();
  uint32_t height = m_engine_window->get_height();

  float aspectRatio =
      (height > 0) ? (static_cast<float>(width) / static_cast<float>(height))
                   : 1.0f;

  if (m_use_editor_camera) {
    viewMatrix = m_editor_camera.getViewMatrix();
    projectionMatrix = m_editor_camera.getProjectionMatrix(aspectRatio);
  } else {
    entt::entity activeCam = m_active_scene->getActiveRuntimeCamera();
    if (activeCam != entt::null) {
      auto &reg = m_active_scene->registry();
      if (reg.all_of<TransformComponent, CameraComponent>(activeCam)) {
        const auto &camTransform = reg.get<TransformComponent>(activeCam);
        const auto &camera = reg.get<CameraComponent>(activeCam);

        viewMatrix = glm::inverse(camTransform.getTransform());
        projectionMatrix = camera.getProjection(aspectRatio);
      }
    }
  }

  auto view =
      m_active_scene->registry().view<TransformComponent, MeshComponent>();
  for (auto entity : view) {
    const auto &transform = view.get<TransformComponent>(entity);
    const auto &mesh = view.get<MeshComponent>(entity);

    renderQueue.push_back(
        {.handle = mesh.handle, .transform = transform.getTransform()});
  }

  m_renderer->present(renderQueue, viewMatrix, projectionMatrix);
}

} // namespace ob
