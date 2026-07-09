#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ob {

class EditorCamera {
public:
  EditorCamera() { updateCameraVectors(); }

  glm::vec3 position{0.0f, 0.5f, 5.0f};

  glm::vec3 rotation{-0.0f, -0.0f, 0.0f};

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
    proj[1][1] *= -1.0f; // Standard Vulkan Y-axis viewport flip
    return proj;
  }

  void moveForward(float speed) {
    // TODO: correct this if you know why it goes the wrong way without the
    // flipping
    auto forward = m_front;
    forward.x *= -1;
    position += forward * speed;
  }
  void moveRight(float speed) {
    // TODO: do the same for this
    auto right = m_right;
    right.z *= -1;
    position += right * speed;
  }

  void moveUp(float speed) { position += m_up * speed; }

  void processMouseMovement(float xOffset, float yOffset) {
    rotation.y += xOffset;
    rotation.x += yOffset;

    if (rotation.x > 89.0f)
      rotation.x = 89.0f;
    if (rotation.x < -89.0f)
      rotation.x = -89.0f;

    updateCameraVectors();
  }

private:
  const glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

  void updateCameraVectors() {
    glm::vec3 front;
    front.x = sin(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
    front.y = sin(glm::radians(rotation.x));
    front.z = -cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
  }
};

} // namespace ob
