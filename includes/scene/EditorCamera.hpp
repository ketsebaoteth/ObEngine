#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ob {

class EditorCamera {
public:
  EditorCamera() { updateCameraVectors(); }

  glm::vec3 position{0.0f, 0.0f, 5.0f};
  glm::vec3 rotation{0.0f, -90.0f, 0.0f}; // Pitch, Yaw, Roll
  float fov = 45.0f;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;

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
    rotation.y += xOffset;
    rotation.x += yOffset;

    if (rotation.x > 89.0f)
      rotation.x = 89.0f;
    if (rotation.x < -89.0f)
      rotation.x = -89.0f;

    updateCameraVectors();
  }

private:
  glm::vec3 m_front{0.0f, 0.0f, -1.0f};
  glm::vec3 m_right{1.0f, 0.0f, 0.0f};
  glm::vec3 m_up{0.0f, 1.0f, 0.0f};
  const glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

  void updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
    front.y = sin(glm::radians(rotation.x));
    front.z = sin(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));

    m_front = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
  }
};

} // namespace ob
