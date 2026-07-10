#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 fragWorldPos;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 lightPosRange;
} push;

void main() {
    float dist = length(fragWorldPos - push.lightPosRange.xyz);
    gl_FragDepth = dist / push.lightPosRange.w; 
}
