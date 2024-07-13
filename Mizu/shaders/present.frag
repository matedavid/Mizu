#version 450

layout (set = 0, binding = 0) uniform sampler2D uPresentTexture;

layout (location = 0) in vec2 vTextureCoords;

layout (location = 0) out vec4 OutColor;

void main() {
    OutColor = texture(uPresentTexture, vTextureCoords);
}