#pragma once
#include "glm/ext/matrix_float4x4.hpp"
#include <array>
#include <string>
// clang-format off
#include <unordered_map>
#include <volk.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"
// clang-format on
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ob {

using MeshHandle = uint32_t;

struct TagComponent {
  std::string tag;
  TagComponent(const std::string &tagStr) : tag(tagStr) {}
  TagComponent() = default;
};

struct PointLightComponent {
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float intensity{1.0f};
  float range{10.0f};
};

using MaterialHandle = uint32_t;

struct PBRMaterialComponent {
  std::string name = "New Material";

  MaterialHandle materialHandle{0};

  std::unordered_map<std::string, float> scalarParameters = {
      {"metallic", 0.0f}, {"roughness", 0.5f}, {"emissionStrength", 1.0f}};

  std::unordered_map<std::string, glm::vec4> vectorParameters = {
      {"baseColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.00f)},
      {"emissionColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.00f)}};

  std::unordered_map<std::string, std::string> textureParameters = {
      {"albedoMap", ""},
      {"normalMap", ""},
      {"metallicRoughnessMap", ""},
      {"emissionMap", ""}};
};

struct TransformComponent {
  glm::vec3 translation{0.0f};
  glm::vec3 rotation{0.0f};
  glm::vec3 scale{1.0f};

  glm::mat4 getTransform() const {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), translation);
    mat = glm::rotate(mat, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    mat = glm::rotate(mat, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    mat = glm::rotate(mat, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::scale(mat, scale);
  }
};
struct MeshComponent {
  MeshHandle handle{0};
};

struct CameraComponent {
  enum class ProjectionType { Perspective = 0, Orthographic = 1 };

  ProjectionType projection_type = ProjectionType::Perspective;

  glm::mat4 view_matrix{1.0f};
  float fov = 45.0f;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;

  float ortho_size = 10.0f;

  bool is_active = false;

  [[nodiscard]] glm::mat4 getProjection(float aspectRatio) const {
    if (projection_type == ProjectionType::Perspective) {
      auto proj = glm::perspective(glm::radians(fov), aspectRatio, near_plane,
                                   far_plane);
      proj[1][1] *= -1.0f;
      return proj;
    } else {
      float orthoLeft = -ortho_size * aspectRatio * 0.5f;
      float orthoRight = ortho_size * aspectRatio * 0.5f;
      float orthoBottom = ortho_size * 0.5f;
      float orthoTop = -ortho_size * 0.5f;
      auto proj = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop,
                             near_plane, far_plane);
      return proj;
    }
  }
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 color;
  glm::vec2 uv;
  glm::vec3 normal;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 4>
  getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, uv);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, normal);

    return attributeDescriptions;
  }
};

} // namespace ob
