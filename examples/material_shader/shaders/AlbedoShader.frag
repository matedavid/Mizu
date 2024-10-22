#version 450

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) out vec4 ResultColor;

layout (set = 1, binding = 0) uniform sampler2D uAlbedo;

void main() {
    ResultColor = texture(uAlbedo, vTexCoord);
}