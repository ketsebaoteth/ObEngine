// In editor_inspector.cpp
#include "glm/ext/vector_float3.hpp"
#include "scene/Components.hpp"
#include "ui/editor/editor_layer.hpp"
#include <cstring>
#include <entt/entity/entity.hpp>
#include <imgui.h>

namespace ob {

// Standard Vec3 table (Unchanged, remains perfect!)
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

  if (m_selected_entity != entt::null && registry.valid(m_selected_entity)) {

    // --- 1. TAG COMPONENT ---
    if (registry.all_of<TagComponent>(m_selected_entity)) {
      auto &tag = registry.get<TagComponent>(m_selected_entity).tag;
      if (ImGui::InputText("Tag", tag.data(), tag.capacity() + 1)) {
        tag.resize(strlen(tag.c_str()));
      }
      ImGui::Separator();
    }

    // --- 2. TRANSFORM COMPONENT ---
    if (registry.all_of<TransformComponent>(m_selected_entity)) {
      auto &transform = registry.get<TransformComponent>(m_selected_entity);

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

      ImGui::Separator();
    }

    // --- 3. POINT LIGHT COMPONENT ---
    if (registry.all_of<PointLightComponent>(m_selected_entity)) {
      auto &light = registry.get<PointLightComponent>(m_selected_entity);

      ImGui::Text("Point Light");
      ImGui::ColorEdit3("Color", &light.color.x);
      ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f,
                       "%.2f");
      ImGui::DragFloat("Range", &light.range, 0.2f, 0.0f, 1000.0f,
                       "%.1f units");

      ImGui::Separator();
    }

    // ========================================================
    // --- 4. NEW FORWARD+ PBR MATERIAL COMPONENT EDITOR ---
    // ========================================================
    if (registry.all_of<PBRMaterialComponent>(m_selected_entity)) {
      auto &mat = registry.get<PBRMaterialComponent>(m_selected_entity);

      ImGui::Text("PBR Material: %s", mat.name.c_str());
      ImGui::Spacing();

      // --- PROPERTY A: BASE COLOR (VECTOR 4) ---
      {
        ImGui::Text("Base Color");
        bool hasAlbedoMap = (mat.textureParameters["albedoMap"] != "");

        if (hasAlbedoMap) {
          // If texture is active, display the path and a release button
          ImGui::PushStyleColor(
              ImGuiCol_Text,
              ImVec4(0.3f, 0.7f, 1.0f, 1.0f)); // Style blue for file links
          ImGui::Text("Linked: %s", mat.textureParameters["albedoMap"].c_str());
          ImGui::PopStyleColor();
        } else {
          // Otherwise, draw the standard color picker
          ImGui::ColorEdit4("##BaseColorPicker",
                            &mat.vectorParameters["baseColor"].x);
        }

        ImGui::SameLine();
        if (ImGui::Button("...##BaseColorSettings")) {
          ImGui::OpenPopup("BaseColorPopup");
        }

        if (ImGui::BeginPopup("BaseColorPopup")) {
          if (ImGui::MenuItem("Use Constant Value")) {
            mat.textureParameters["albedoMap"] = ""; // Remove linked texture
          }
          ImGui::Separator();
          ImGui::Text("Assign Texture Map:");

          char pathBuffer[256];
          std::strncpy(pathBuffer, mat.textureParameters["albedoMap"].c_str(),
                       sizeof(pathBuffer));
          if (ImGui::InputText("Path##Albedo", pathBuffer,
                               sizeof(pathBuffer))) {
            mat.textureParameters["albedoMap"] = pathBuffer;
          }
          ImGui::EndPopup();
        }
      }
      ImGui::Spacing();

      {
        ImGui::Text("Metallic");
        bool hasMetallicMap =
            (mat.textureParameters["metallicRoughnessMap"] != "");

        if (hasMetallicMap) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
          ImGui::Text("Linked: Metallic-Roughness Map");
          ImGui::PopStyleColor();
        } else {
          ImGui::SliderFloat("##MetallicSlider",
                             &mat.scalarParameters["metallic"], 0.0f, 1.0f,
                             "%.2f");
        }

        ImGui::SameLine();
        if (ImGui::Button("...##MetallicSettings")) {
          ImGui::OpenPopup("MetallicPopup");
        }

        if (ImGui::BeginPopup("MetallicPopup")) {
          if (ImGui::MenuItem("Use Constant Value")) {
            mat.textureParameters["metallicRoughnessMap"] = "";
          }
          ImGui::Separator();
          ImGui::Text("Assign Texture Map:");
          char pathBuffer[256];
          std::strncpy(pathBuffer,
                       mat.textureParameters["metallicRoughnessMap"].c_str(),
                       sizeof(pathBuffer));
          if (ImGui::InputText("Path##MetRough", pathBuffer,
                               sizeof(pathBuffer))) {
            mat.textureParameters["metallicRoughnessMap"] = pathBuffer;
          }
          ImGui::EndPopup();
        }
      }
      ImGui::Spacing();

      // --- PROPERTY C: ROUGHNESS (SCALAR) ---
      {
        ImGui::Text("Roughness");
        bool hasRoughnessMap =
            (mat.textureParameters["metallicRoughnessMap"] != "");

        if (hasRoughnessMap) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
          ImGui::Text("Linked: Metallic-Roughness Map");
          ImGui::PopStyleColor();
        } else {
          ImGui::SliderFloat("##RoughnessSlider",
                             &mat.scalarParameters["roughness"], 0.0f, 1.0f,
                             "%.2f");
        }

        ImGui::SameLine();
        if (ImGui::Button("...##RoughnessSettings")) {
          ImGui::OpenPopup("RoughnessPopup");
        }

        if (ImGui::BeginPopup("RoughnessPopup")) {
          if (ImGui::MenuItem("Use Constant Value")) {
            mat.textureParameters["metallicRoughnessMap"] = "";
          }
          ImGui::Separator();
          ImGui::Text("Assign Texture Map:");
          char pathBuffer[256];
          std::strncpy(pathBuffer,
                       mat.textureParameters["metallicRoughnessMap"].c_str(),
                       sizeof(pathBuffer));
          if (ImGui::InputText("Path##MetRough2", pathBuffer,
                               sizeof(pathBuffer))) {
            mat.textureParameters["metallicRoughnessMap"] = pathBuffer;
          }
          ImGui::EndPopup();
        }
      }
      ImGui::Spacing();

      // --- PROPERTY D: EMISSION (VECTOR + STRENGTH) ---
      {
        ImGui::Text("Emission");
        ImGui::ColorEdit3("Color##Emission",
                          &mat.vectorParameters["emissionColor"].x);
        ImGui::DragFloat("Strength##Emission",
                         &mat.scalarParameters["emissionStrength"], 0.1f, 0.0f,
                         100.0f, "%.1f");
      }

      ImGui::Separator();
    }
    // ========================================================

  } else {
    ImGui::Text("No Entity Selected");
  }

  ImGui::End();
}

} // namespace ob
