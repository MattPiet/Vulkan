#version 450
#extension GL_ARB_separate_shader_objects : enable
#define MAX_LIGHTS 4

layout(location = 0) in vec4 vVertex;
layout(location = 1) in vec4 vNormal;
layout(location = 2) in vec2 texCoords;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 previousProjectionMatrix;
    mat4 previousViewMatrix;
} camera;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat3x4 normalMatrix;
    vec4 pad;
    mat4 previousModelMatrix;
} push;

layout(set = 1, binding = 0) uniform LightsUBO {
    vec4 pos[MAX_LIGHTS];
    vec4 diffuse[MAX_LIGHTS];
    vec4 specular[MAX_LIGHTS];
    vec4 ambient;
    uint numLights;
} lights;

layout(location = 0) out vec3 vertNormal;
layout(location = 1) out vec3 eyeDir;
layout(location = 2) out vec2 fragTexCoords;
layout(location = 3) out vec3 lightDir[MAX_LIGHTS];
layout(location = 7) noperspective out vec2 currentNdc;
layout(location = 8) out vec4 previousClipPosition;

void main() {
    fragTexCoords = texCoords;
    vertNormal = normalize(mat3(push.normalMatrix) * vNormal.xyz);

    vec3 vertPos = vec3(push.modelMatrix * vVertex);
    vec3 camPos = inverse(camera.viewMatrix)[3].xyz;
    eyeDir = normalize(camPos - vertPos);
    for (uint i = 0; i < lights.numLights; ++i) {
        lightDir[i] = vec3(lights.pos[i]) - vertPos;
    }

    vec4 currentClip = camera.projectionMatrix * camera.viewMatrix * push.modelMatrix * vVertex;
    vec4 previousClip = camera.previousProjectionMatrix * camera.previousViewMatrix * push.previousModelMatrix * vVertex;
    currentNdc = (abs(currentClip.w) > 1e-5)
        ? currentClip.xy / currentClip.w
        : vec2(0.0);
    // Perspective-correctly interpolate the previous frame's clip position.
    previousClipPosition = previousClip;
    gl_Position = currentClip;
}
