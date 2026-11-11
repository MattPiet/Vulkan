#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_query : require

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

// Ray Tracing Acceleration Structure bound to Set 1, Binding 2
layout(set = 1, binding = 2) uniform accelerationStructureEXT topLevelAS;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 outMotionVector;

void main() {
    vec3 normal = normalize(vertNormal);
    vec3 eyeDirection = normalize(eyeDir);

    vec4 ka = lights.ambient;
    vec4 kt = texture(texSampler, fragTexCoords);
    vec4 phongResult = vec4(0.0);

    float constant = 1.0;
    float linear = 0.09;
    float quadratic = 0.032;

    for (uint i = 0; i < lights.numLights; ++i) {
        float distance = length(lightDir[i]);
        vec3 lightDirection = normalize(lightDir[i]);

        // Derive fragment world position from light vector
        vec3 worldPos = lights.pos[i].xyz - lightDir[i];

        // Ray origin biased along surface normal to prevent shadow acne
        vec3 rayOrigin = worldPos + normal * 0.001;

        // Trace shadow ray query toward light source
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(
                rayQuery,
                topLevelAS,
                gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF,
                rayOrigin,
                0.001,          // tMin
                lightDirection, // Ray direction
                distance        // tMax (stop tracing at light position)
        );

        while (rayQueryProceedEXT(rayQuery)) {}

        float shadow = 1.0;
        if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
            shadow = 0.0; // In shadow
        }

        float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
        vec4 ks = lights.specular[i];
        vec4 kd = lights.diffuse[i];

        float diff = max(dot(normal, lightDirection), 0.0);
        vec3 reflection = normalize(reflect(-lightDirection, normal));
        float spec = pow(max(dot(eyeDirection, reflection), 0.0), 14.0);

        phongResult += ((diff * kd * kt * attenuation) + (spec * ks * attenuation)) * shadow;
    }

    fragColor = phongResult + (ka * kt);

    // DLSS Motion Vector pass
    if (abs(previousClipPosition.w) > 1e-5) {
        vec2 previousNdc = previousClipPosition.xy / previousClipPosition.w;
        outMotionVector = (previousNdc - currentNdc) * 0.5;
    } else {
        outMotionVector = vec2(0.0);
    }
}