#include "ui/editor/editor_layer.hpp"
#include <imgui.h>

namespace ob {
void EditorLayer::drawHierarchyPanel() {
  ImGui::Begin("Scene Hierarchy");
  auto &registry = m_scene->registry();
  auto view = registry.view<TagComponent>();

  for (auto entity : view) {
    const std::string &tag = view.get<TagComponent>(entity).tag;

    ImGuiTreeNodeFlags flags =
        ((m_selected_entity == entity) ? ImGuiTreeNodeFlags_Selected : 0);
    flags |= ImGuiTreeNodeFlags_OpenOnArrow |
             ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx(
        reinterpret_cast<void *>(static_cast<uintptr_t>(entity)), flags, "%s",
        tag.c_str());

    if (ImGui::IsItemClicked()) {
      m_selected_entity = entity;
    }
    if (opened) {
      ImGui::TreePop();
    }
  }

  if (ImGui::BeginPopupContextWindow(nullptr,
                                     ImGuiPopupFlags_MouseButtonRight |
                                         ImGuiPopupFlags_NoOpenOverItems)) {
    if (ImGui::MenuItem("Create Empty Entity")) {
      auto newEntity = m_scene->createEntity();
      m_selected_entity = newEntity;
    }
    m_popup_opened = false;
    ImGui::EndPopup();
  }

  ImGui::End();
}
} // namespace ob
