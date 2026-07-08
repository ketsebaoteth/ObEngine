#pragma once
#include "engine_events.hpp" // All Event structs and enums come from here!
#include "glm/ext/vector_float2.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ob {

struct ActionDetails {
  glm::vec2 mouseLandPos;
};

struct InputBinding {
  EventType type;
  int32_t code;
  std::string actionName;
};

class EventManager {
public:
  EventManager();
  ~EventManager();

  void bindAction(EventType type, int32_t code, const std::string &actionName) {
    m_bindings.push_back({type, code, actionName});
  }

  void registerActionCallback(const std::string &actionName,
                              std::function<void(ActionDetails)> callback) {
    m_actions[actionName] = callback;
  }

  void loadEngineActions();
  void update(const std::vector<Event> &events);

private:
  std::unordered_map<std::string, std::function<void(ActionDetails)>> m_actions;
  std::vector<InputBinding> m_bindings;
};

} // namespace ob
