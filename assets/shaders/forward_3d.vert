#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 outPosition; 
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 outNormal;

layout(binding = 0) uniform GlobalUbo {
    mat4 view;
    mat4 proj;
    vec2 screenSize;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor;
    vec3 emissionColor;
    float emissionStrength;
    float metallic;
    float roughness;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    outPosition = worldPos.xyz;

    outNormal = mat3(transpose(inverse(push.model))) * inNormal;
    
    outColor = inColor;
    outUV = inUV;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
