#include "engine/layer.hpp"
#include "scene/Scene.hpp"
#include <memory>
#include <vector>

namespace ob {

class IWindow;
class IRenderer;
class EventManager;
class ImGuiLayer;

class LayerManager {
public:
  LayerManager(IWindow *window, IRenderer *renderer, EventManager *eventManager,
               Scene *scene);
  ~LayerManager();

  void update(float deltaTime);
  void render();

private:
  std::vector<std::unique_ptr<Layer>> m_layers;
  ImGuiLayer *m_imgui_layer = nullptr;
};
} // namespace ob
