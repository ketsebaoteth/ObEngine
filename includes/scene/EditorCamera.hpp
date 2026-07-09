#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace ob {
class EditorCamera {
public:
  EditorCamera() : rotation(0.0f, 0.0f, 0.0f) { updateCameraVectors(); }
  glm::vec3 position{0.0f, 0.5f, 5.0f};
  glm::vec3 rotation; // X = Pitch, Y = Yaw, Z = Roll (degrees)
  float fov = 45.0f;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;
  glm::vec3 m_front{0.0f, 0.0f, -1.0f};
  glm::vec3 m_right{1.0f, 0.0f, 0.0f};
  glm::vec3 m_up{0.0f, 1.0f, 0.0f};
  [[nodiscard]] glm::mat4 getViewMatrix() const {
    return glm::lookAt(position, position + m_front, m_up);
  }
  [[nodiscard]] glm::mat4 getProjectionMatrix(float aspectRatio) const {
    auto proj =
        glm::perspective(glm::radians(fov), aspectRatio, near_plane, far_plane);
    proj[1][1] *= -1.0f;
    return proj;
  }
  void moveForward(float speed) { position += m_front * speed; }
  void moveRight(float speed) { position += m_right * speed; }
  void moveUp(float speed) { position += m_up * speed; }
  void processMouseMovement(float xOffset, float yOffset) {
    rotation.y += xOffset; // Yaw
    rotation.x += yOffset; // Pitch
    if (rotation.x > 89.0f)
      rotation.x = 89.0f;
    if (rotation.x < -89.0f)
      rotation.x = -89.0f;
    updateCameraVectors();
  }

private:
  const glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};
  void updateCameraVectors() {
    // Matches TransformComponent::getTransform()'s T * Rx(pitch) * Ry(yaw) *
    // Rz(roll) applied to local forward (0,0,-1). Roll doesn't affect forward
    // (rotates around it), so it's intentionally absent here.
    float yaw = glm::radians(rotation.y);
    float pitch = glm::radians(rotation.x);

    glm::vec3 front;
    front.x = -sin(yaw);
    front.y = sin(pitch) * cos(yaw);
    front.z = -cos(pitch) * cos(yaw);
    m_front = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
  }
};
} // namespace ob
