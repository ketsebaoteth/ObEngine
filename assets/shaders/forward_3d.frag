#version 450

// --- INPUTS ---
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal; // --- ADDED INPUT LOCATION 3 ---

// --- OUTPUTS ---
layout(location = 0) out vec4 outColor;

// --- BINDINGS ---
layout(binding = 0) uniform GlobalUbo {
    mat4 view;
    mat4 proj;
    vec2 screenSize;
} ubo;

struct PointLight {
    vec4 positionAndRange;  // xyz = pos, w = range
    vec4 colorAndIntensity; // xyz = color, w = intensity
};

layout(std430, binding = 1) readonly buffer LightBuffer {
    PointLight lights[];
} lightBuffer;

layout(std430, binding = 2) readonly buffer TileLightIndicesBuffer {
    uint data[];
} tileLightIndicesBuffer;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor;
    vec3 emissionColor;
    float emissionStrength;
    float metallic;
    float roughness;
} push;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 1. Calculate tile indices
    ivec2 tileCoords = ivec2(gl_FragCoord.xy) / 16;
    uint numTilesX = (uint(ubo.screenSize.x) + 15) / 16;
    uint flatTileIndex = tileCoords.y * numTilesX + tileCoords.x;
    uint bufferOffset  = flatTileIndex * (1 + 256);

    uint visibleLightCount = tileLightIndicesBuffer.data[bufferOffset];

    vec3 albedo     = push.baseColor.rgb; 
    float metallic  = push.metallic;
    float roughness = push.roughness;

    // --- FIXED BUG: USE SMOOTH INTERPOLATED VERTEX NORMALS ---
    // Zero derivative hacks, zero winding landmines!
    vec3 N = normalize(inNormal); 
    // ---------------------------------------------------------

    vec3 cameraPos = inverse(ubo.view)[3].xyz;
    vec3 V = normalize(cameraPos - inPosition);

    // If the polygon is double-sided, align normal to face the eye
    if (dot(N, V) < 0.0) {
        N = -N; 
    }

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 ambient = vec3(0.03) * albedo;
    vec3 emission = push.emissionColor * push.emissionStrength;
    vec3 accumulatedLighting = ambient + emission;

    // Loop ONLY over lights visible to this tile!
    for (uint i = 0; i < visibleLightCount && i < 256; i++) {
        uint lightIndex = tileLightIndicesBuffer.data[bufferOffset + 1 + i];
        
        vec3 lightPos   = lightBuffer.lights[lightIndex].positionAndRange.xyz;
        float radius    = lightBuffer.lights[lightIndex].positionAndRange.w;
        vec3 lightColor = lightBuffer.lights[lightIndex].colorAndIntensity.xyz;
        float intensity = lightBuffer.lights[lightIndex].colorAndIntensity.w;

        vec3 toLight = lightPos - inPosition;
        float distance = length(toLight);

        if (distance < radius) {
            vec3 L = normalize(toLight);
            vec3 H = normalize(V + L);

            float NdotL = max(dot(N, L), 0.0);
            float NdotV = max(dot(N, V), 0.0);

            float attenuation = clamp(1.0 - (distance / radius), 0.0, 1.0);
            attenuation *= attenuation; 

            vec3 radiance = lightColor * intensity * attenuation;

            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 numerator     = D * G * F;
            float denominator  = 4.0 * NdotV * NdotL;
            vec3 specular      = numerator / max(denominator, 0.001);

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            accumulatedLighting += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

    vec3 srgbColor = pow(accumulatedLighting, vec3(1.0 / 2.2));
    outColor = vec4(srgbColor, 1.0);
}
