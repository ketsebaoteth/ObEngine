#pragma once
#include "event/engine_events.hpp"
#include "event/event.hpp"
#include "rhi/renderer.hpp"
#include "windowing/window.hpp"

namespace ob {

class Layer {
public:
  virtual ~Layer() = default;

  virtual void on_attach() {}
  virtual void on_detach() {}
  virtual void on_update(float delta_time) {}
  virtual void on_ui_render() {}
  virtual void on_event(const Event &event) {}

  std::unique_ptr<IRenderer> m_renderer;
};

} // namespace ob
