#version 450

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D uTexture1;

layout (set = 1, binding = 0) uniform sampler2D uTexture2;
layout (set = 1, binding = 1) uniform Uniform1 {
    vec4 Position;
    vec3 Direction;
} uUniform1;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}