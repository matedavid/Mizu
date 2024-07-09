#version 450

layout (set = 1, binding = 0) uniform sampler2D uInput;

layout (location = 0) in vec2 vTexCoord;
layout (location = 0) out vec4 ResultColor;

void main() {
    vec3 color = texture(uInput, vTexCoord).rgb;
    if (color != vec3(0.0, 0.0, 0.0)) {
        color = vec3(1.0) - color;
    }

    ResultColor = vec4(color, 1.0);
}
