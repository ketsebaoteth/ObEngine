#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragWorldPos;

layout(binding = 0) uniform ShadowUbo {
    mat4 shadowMatrices[6];
} shadowUbo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 lightPosRange;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = shadowUbo.shadowMatrices[gl_ViewIndex] * worldPos;
}
