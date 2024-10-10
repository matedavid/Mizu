#version 450

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (set = 0, binding = 0) uniform CameraInfo {
    mat4 view;
    mat4 projection;
} uCameraInfo;

layout (push_constant) uniform ModelInfoPushConstants {
    mat4 model;
} uModelInfo;

layout (location = 0) out vec3 vTextureCoords;

void main() {
    vTextureCoords = aPosition;

    mat4 view = mat4(mat3(uCameraInfo.view));
    vec4 pos = uCameraInfo.projection * view * uModelInfo.model * vec4(aPosition, 1.0f);
    gl_Position = pos.xyww;
}