#version 450

// --- INPUTS (Matches your Vertex struct attributes!) ---
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

// --- OUTPUTS (Must match fragment shader inputs exactly!) ---
layout(location = 0) out vec3 outPosition; // World-space position for distance calculations
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 outNormal;

// --- BINDINGS ---
layout(binding = 0) uniform GlobalUbo {
    mat4 view;
    mat4 proj;
    vec2 screenSize;
} ubo;

// Matches your 64-byte Push Constant range we verified earlier!
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor;
    vec3 emissionColor;
    float emissionStrength;
    float metallic;
    float roughness;
} push;

void main() {
    // 1. Calculate world-space coordinates
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    outPosition = worldPos.xyz;

    outNormal = mat3(transpose(inverse(push.model))) * inNormal;
    // 2. Symmetrically pass color and UV to the rasterizer
    outColor = inColor;
    outUV = inUV;

    // 3. Output hardware clip coordinates
    gl_Position = ubo.proj * ubo.view * worldPos;
}
