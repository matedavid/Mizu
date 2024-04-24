#version 450

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D uTexture1;

layout (set = 1, binding = 0) uniform sampler2D uTexture2;
layout (set = 1, binding = 1) uniform Uniform1 {
    vec4 Position;
    vec3 Direction;
} uUniform1;

layout (push_constant) uniform PushConstant {
    vec4 value;
} uConstant1;

void main() {
    vec4 t1 = texture(uTexture1, vec2(0.0));
    vec4 t2 = texture(uTexture2, vec2(0.0));
    outColor = t1+t2;
}