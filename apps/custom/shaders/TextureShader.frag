#version 450

layout (location = 0) in vec3 vNormal;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 ResultColor;

layout (set = 1, binding = 0) uniform sampler2D uTexture;

void main() {
    vec3 lightPos = vec3(2, 1, 2);
    float cosTheta = clamp(dot(vNormal, lightPos), 0.3, 1.0);

    vec4 color = texture(uTexture, vTexCoord);
    ResultColor = color * cosTheta;
}
