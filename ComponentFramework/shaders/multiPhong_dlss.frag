#version 450
#extension GL_ARB_separate_shader_objects : enable
#define MAX_LIGHTS 4

layout(location = 0) in vec3 vertNormal;
layout(location = 1) in vec3 eyeDir;
layout(location = 2) in vec2 fragTexCoords;
layout(location = 3) in vec3 lightDir[MAX_LIGHTS];
layout(location = 7) noperspective in vec2 currentNdc;
layout(location = 8) in vec4 previousClipPosition;

layout(set = 1, binding = 0) uniform LightsUBO {
    vec4 pos[MAX_LIGHTS];
    vec4 diffuse[MAX_LIGHTS];
    vec4 specular[MAX_LIGHTS];
    vec4 ambient;
    uint numLights;
} lights;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 outMotionVector;
layout(set = 2, binding = 0) uniform sampler2D texSampler;

void main() {
    vec3 normal = normalize(vertNormal);
    vec3 eyeDirection = normalize(eyeDir);
    vec3 reflection[MAX_LIGHTS];
    float spec[MAX_LIGHTS];
    float diff[MAX_LIGHTS];

    vec4 ka = lights.ambient;
    vec4 kt = texture(texSampler, fragTexCoords);
    vec4 phongResult = vec4(0.0);
    float constant = 1.0;
    float linear = 0.09;
    float quadratic = 0.032;

    for (uint i = 0; i < lights.numLights; ++i) {
        float distance = length(lightDir[i]);
        vec3 lightDirection = normalize(lightDir[i]);
        float attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
        vec4 ks = lights.specular[i];
        vec4 kd = lights.diffuse[i];
        diff[i] = max(dot(normal, lightDirection), 0.0);
        reflection[i] = normalize(reflect(-lightDirection, normal));
        spec[i] = max(dot(eyeDirection, reflection[i]), 0.0);
        spec[i] = pow(spec[i], 14.0);
        phongResult += (diff[i] * kd * kt * attenuation) + (spec[i] * ks * attenuation);
    }
    fragColor = phongResult + (ka * kt);

    // Streamline expects motion towards the previous frame. Vectors are normalized
    // UV offsets, so the renderer supplies mvecScale = {1, 1}.
    if (abs(previousClipPosition.w) > 1e-5) {
        vec2 previousNdc = previousClipPosition.xy / previousClipPosition.w;
        outMotionVector = (previousNdc - currentNdc) * 0.5;
    } else {
        outMotionVector = vec2(0.0);
    }
}
