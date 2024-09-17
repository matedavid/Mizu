#version 450

layout (location = 0) in vec3 vNormal;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 ResultColor;

void main() {
    ResultColor = mix(vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), vTexCoord.x);
}
