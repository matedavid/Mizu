#version 450

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) out vec2 vTexCoord;

layout (set = 0, binding = 0) uniform CameraInfo {
    mat4 view;
    mat4 projection;
} uCameraInfo;

layout (push_constant) uniform ModelInfo {
    mat4 model;
} uModelInfo;

void main() {
    gl_Position = uCameraInfo.projection * uCameraInfo.view * uModelInfo.model * vec4(aPosition, 1.0);

    vTexCoord = aTexCoord;
}