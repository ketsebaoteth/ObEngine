#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
} pcs;

void main() {
    gl_Position = ubo.proj * ubo.view * pcs.modelMatrix * vec4(inPosition, 1.0);
    
    fragColor = inColor;
}
