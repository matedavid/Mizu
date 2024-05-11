#version 450

layout (set = 1, binding = 0) uniform sampler2D uColorTexture;
layout (set = 1, binding = 1) uniform AttenuationInfo {
    vec4 attenuation;
} uAttenuationInfo;

layout (location = 0) in vec2 vTextureCoords;

layout (location = 0) out vec4 ResultColor;

void main() {
    vec4 color = texture(uColorTexture, vTextureCoords);
    ResultColor = vec4(color.b, color.g, color.r, 1.0) * uAttenuationInfo.attenuation;
}
