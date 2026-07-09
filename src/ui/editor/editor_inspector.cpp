#include "glm/ext/vector_float3.hpp"
#include "scene/Components.hpp"
#include "ui/editor/editor_layer.hpp"
#include <entt/entity/entity.hpp>
#include <imgui.h>

namespace ob {

static void drawVec3Table(const std::string &tablename, glm::vec3 &data,
                          const char *formatX, const char *formatY,
                          const char *formatZ, float speed = 0.1f) {
  if (ImGui::BeginTable(tablename.c_str(), 3,
                        ImGuiTableFlags_NoSavedSettings)) {
    std::string idX = "##X_" + tablename;
    std::string idY = "##Y_" + tablename;
    std::string idZ = "##Z_" + tablename;

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1.0f);
    ImGui::DragFloat(idX.c_str(), &data.x, speed, 0.0f, 0.0f, formatX);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1.0f);
    ImGui::DragFloat(idY.c_str(), &data.y, speed, 0.0f, 0.0f, formatY);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1.0f);
    ImGui::DragFloat(idZ.c_str(), &data.z, speed, 0.0f, 0.0f, formatZ);
    ImGui::PopItemWidth();

    ImGui::EndTable();
  }
}
void EditorLayer::drawInspectorPanel() {
  ImGui::Begin("Inspector");

  auto &registry = m_scene->registry();
  if (m_selected_entity != entt::null &&
      registry.all_of<TransformComponent>(m_selected_entity)) {
    auto &transform = registry.get<TransformComponent>(m_selected_entity);
    auto &tag = registry.get<TagComponent>(m_selected_entity).tag;
    if (ImGui::InputText("Tag", tag.data(), tag.capacity() + 1)) {
      tag.resize(strlen(tag.c_str()));
    }

    ImGui::Text("Translation");
    drawVec3Table("TranslationColumn", transform.translation, "X: %.2f",
                  "Y: %.2f", "Z: %.2f", 0.1f);

    ImGui::Text("Rotation");
    glm::vec3 rotationDegrees = glm::degrees(transform.rotation);
    drawVec3Table("RotationColumn", rotationDegrees, "X: %.1f°", "Y: %.1f°",
                  "Z: %.1f°", 1.0f);
    transform.rotation = glm::radians(rotationDegrees);

    ImGui::Text("Scale");
    drawVec3Table("ScaleColumn", transform.scale, "X: %.2f", "Y: %.2f",
                  "Z: %.2f", 0.05f);
  } else {
    ImGui::Text("No Entity Selected");
  }

  ImGui::End();
}

} // namespace ob
