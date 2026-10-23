#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 outUVW;
layout(location = 1) noperspective in vec2 currentNdc;
layout(location = 2) in vec4 previousClipPosition;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 outMotionVector;
layout(set = 1, binding = 0) uniform samplerCube skyboxSampler;

void main() {
    fragColor = texture(skyboxSampler, outUVW);
    if (abs(previousClipPosition.w) > 1e-5) {
        vec2 previousNdc = previousClipPosition.xy / previousClipPosition.w;
        outMotionVector = (previousNdc - currentNdc) * 0.5;
    } else {
        outMotionVector = vec2(0.0);
    }
}
