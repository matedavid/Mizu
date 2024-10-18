#version 450
layout (location = 0) in vec3 vNormal;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 ResultColor;

layout (set = 1, binding = 0) uniform sampler2D albedo;
layout (set = 1, binding = 1) uniform sampler2D metallic;

void main() {
    vec3 lightPos = vec3(2, 1, 2);
    float cosTheta = clamp(dot(vNormal, lightPos), 0.3, 1.0);

    vec4 color = texture(albedo, vTexCoord);
    vec4 met = texture(metallic, vTexCoord);
    ResultColor = color * cosTheta * met;
}
