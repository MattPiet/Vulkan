#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 vNormal;
layout(location = 2) in vec2 texCoords;
layout(location = 0) out vec3 outUVW;
layout(location = 1) noperspective out vec2 currentNdc;
layout(location = 2) out vec4 previousClipPosition;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 previousProjectionMatrix;
    mat4 previousViewMatrix;
} camera;

void main() {
    outUVW = inPosition;
    // A skybox is at infinity: camera rotation moves it, camera translation does not.
    mat4 currentViewRotation = mat4(mat3(camera.viewMatrix));
    mat4 previousViewRotation = mat4(mat3(camera.previousViewMatrix));
    vec4 currentClip = camera.projectionMatrix * currentViewRotation * vec4(inPosition, 1.0);
    vec4 previousClip = camera.previousProjectionMatrix * previousViewRotation * vec4(inPosition, 1.0);
    currentNdc = (abs(currentClip.w) > 1e-5)
        ? currentClip.xy / currentClip.w
        : vec2(0.0);
    previousClipPosition = previousClip;
    // Force depth to far plane as in the original skybox shader.
    gl_Position = currentClip.xyww;
}
