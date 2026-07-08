#pragma once

#include "engine/layer.hpp"
#include <memory>
#include <vector>

namespace ob {

class LayerStack {
public:
  LayerStack() = default;
  ~LayerStack() {
    for (auto &layer : m_layers) {
      layer->on_detach();
    }
  }

  void push_layer(std::shared_ptr<Layer> layer) {
    m_layers.emplace(m_layers.begin() + m_layer_insert_index, layer);
    m_layer_insert_index++;
    layer->on_attach();
  }

  void push_overlay(std::shared_ptr<Layer> overlay) {
    m_layers.emplace_back(overlay);
    overlay->on_attach();
  }

  // Standard iterator accessors so the engine loop can traverse them
  std::vector<std::shared_ptr<Layer>>::iterator begin() {
    return m_layers.begin();
  }
  std::vector<std::shared_ptr<Layer>>::iterator end() { return m_layers.end(); }

private:
  std::vector<std::shared_ptr<Layer>> m_layers;
  unsigned int m_layer_insert_index = 0;
};

} // namespace ob
