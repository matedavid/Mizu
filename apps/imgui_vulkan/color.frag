#version 450

layout (location = 0) in vec2 vTextureCoords;

layout (push_constant) uniform ColorInfo {
    vec3 color;
} uColorInfo;

layout (location = 0) out vec4 ResultColor;

void main() {
    ResultColor = vec4(uColorInfo.color, 1.0);
}
