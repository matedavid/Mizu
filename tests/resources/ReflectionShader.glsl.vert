#version 450

layout (set = 0, binding = 0) uniform Types {
    float f;

    vec2 f2;
    vec3 f3;
    vec4 f4;

    mat3x3 f3x3;
    mat4x4 f4x4;
} types;

void main() {
    float rf = types.f + types.f2.x * types.f3.y + types.f4.z * types.f3x3[1][0] + types.f4x4[0][1];
    gl_Position = vec4(rf, rf, rf, rf);

}