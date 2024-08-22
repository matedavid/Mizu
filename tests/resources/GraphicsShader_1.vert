#version 450

layout (location = 0) in vec3 uPosition;
layout (location = 1) in vec2 uTexCoord;

layout (location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(uPosition, 1.0);
    vTexCoord = uTexCoord;
}