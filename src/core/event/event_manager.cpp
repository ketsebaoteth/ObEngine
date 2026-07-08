// In event_manager.cpp
#include "event/event_manager.hpp"
#include "event/engine_events.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>

namespace ob {

EventManager::EventManager() { loadEngineActions(); };
EventManager::~EventManager() {};

void EventManager::update(const std::vector<Event> &events) {
  ImGuiIO *io = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;

  for (const auto &event : events) {
    if (io) {
      bool isMouseEvent = (event.type == EventType::MouseButtonPressed ||
                           event.type == EventType::MouseButtonReleased ||
                           event.type == EventType::MouseMoved ||
                           event.type == EventType::MouseScrolled);

      bool isKeyboardEvent = (event.type == EventType::KeyPressed ||
                              event.type == EventType::KeyReleased ||
                              event.type == EventType::KeyRepeat);

      bool isRmbClick = (event.type == EventType::MouseButtonPressed ||
                         event.type == EventType::MouseButtonReleased) &&
                        event.mouse_button.button == 1;

      bool isRmbHeld = io->MouseDown[1];

      if (isMouseEvent && io->WantCaptureMouse) {
        if (!isRmbClick && !isRmbHeld) {
          continue;
        }
      }

      if (isKeyboardEvent && io->WantTextInput) {
        continue;
      }
    }

    for (const auto &binding : m_bindings) {
      bool isTypeMatch = event.type == binding.type;
      bool isCodeMatch = (event.type == EventType::MouseMoved) ||
                         (event.key.code == binding.code);

      if (isTypeMatch && isCodeMatch) {
        if (m_actions.contains(binding.actionName)) {
          ActionDetails details{};

          if (event.type == EventType::MouseMoved) {
            details.mouseLandPos =
                glm::vec2(event.mouse_moved.x, event.mouse_moved.y);
          }

          m_actions[binding.actionName](details);
        }
      }
    }
  }
}

void EventManager::loadEngineActions() {
  bindAction(EventType::KeyPressed, 87, "MoveForwardStart");
  bindAction(EventType::KeyReleased, 87, "MoveForwardStop");
  bindAction(EventType::KeyPressed, 83, "MoveBackwardStart");
  bindAction(EventType::KeyReleased, 83, "MoveBackwardStop");
  bindAction(EventType::KeyPressed, 65, "MoveLeftStart");
  bindAction(EventType::KeyReleased, 65, "MoveLeftStop");
  bindAction(EventType::KeyPressed, 68, "MoveRightStart");
  bindAction(EventType::KeyReleased, 68, "MoveRightStop");

  bindAction(EventType::MouseButtonPressed, 1, "LookAroundStart");
  bindAction(EventType::MouseButtonReleased, 1, "LookAroundStop");
  bindAction(EventType::MouseMoved, -1, "LookAroundMove");
}

} // namespace ob
