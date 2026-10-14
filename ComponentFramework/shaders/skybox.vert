#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPosition; // Position of the cube mesh
layout (location = 1) in  vec4 vNormal;
layout (location = 2) in  vec2 texCoords;

layout (location = 0) out vec3 outUVW;   // 3D texture coordinate passed to fragment shader

layout(binding = 0) uniform CameraUBO {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} camera;

void main() {
    outUVW = inPosition;

    // Strip translation from the view matrix so the skybox stays centered around the camera
    mat4 viewRotOnly = mat4(mat3(camera.viewMatrix));

    vec4 clipPos = camera.projectionMatrix * viewRotOnly * vec4(inPosition, 1.0);

    // Force the depth to the maximum (1.0) so it renders behind everything
    gl_Position = clipPos.xyww;
}