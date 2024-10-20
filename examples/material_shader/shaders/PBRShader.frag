#version 450

layout (location = 0) in vec3 vNormal;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 ResultColor;

layout (set = 1, binding = 0) uniform sampler2D uAlbedo;

void main() {
    vec4 color = texture(uAlbedo, vTexCoord);
    ResultColor = color;
}
