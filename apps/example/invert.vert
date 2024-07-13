#version 450

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

layout (set = 0, binding = 0) uniform CameraInfo {
    mat4 view;
    mat4 projection;
} uCameraInfo;

layout (location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
}
