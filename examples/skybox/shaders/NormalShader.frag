#version 450

layout (location = 0) in vec3 vNormal;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 ResultColor;

void main() {
    vec3 lightPos = vec3(2, 1, 2);
    float cosTheta = clamp(dot(vNormal, lightPos), 0.3, 1.0);

    ResultColor = vec4(1.0, 0.0, 0.0, 1.0) * cosTheta;
} 