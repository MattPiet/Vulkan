#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_query : require

#define MAX_LIGHTS 4

layout(location = 0) in vec3 vertNormal;
layout(location = 1) in vec3 eyeDir;
layout(location = 2) in vec2 fragTexCoords;
layout(location = 3) in vec3 lightDir[MAX_LIGHTS];

layout(set = 1, binding = 0) uniform LightsUBO {
	vec4 pos[MAX_LIGHTS];
	vec4 diffuse[MAX_LIGHTS];   // rgb = color, w = brightness/intensity multiplier
	vec4 specular[MAX_LIGHTS];
	vec4 ambient;
	uint numLights;
} lights;

// Ray Tracing Acceleration Structure bound to Set 1, Binding 2
layout(set = 1, binding = 2) uniform accelerationStructureEXT topLevelAS;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 fragColor;

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

		// 1. Skip if surface is facing away from the light source
		float diff = max(dot(normal, lightDirection), 0.0);
		if (diff <= 0.0) {
			continue; // Already in shadow/facing away, skip ray query
		}

		// Derive fragment world position from light vector
		vec3 worldPos = lights.pos[i].xyz - lightDir[i];

		// Ray origin biased along surface normal to prevent shadow acne
		vec3 rayOrigin = worldPos + normal * 0.002;

		// Trace shadow ray query toward light source
		rayQueryEXT rayQuery;
		rayQueryInitializeEXT(
				rayQuery,
				topLevelAS,
				gl_RayFlagsTerminateOnFirstHitEXT |
				gl_RayFlagsOpaqueEXT |
				gl_RayFlagsSkipClosestHitShaderEXT |
				gl_RayFlagsCullBackFacingTrianglesEXT,
				0xFF,
				rayOrigin,
				0.02,
				lightDirection,
				distance
		);

		while (rayQueryProceedEXT(rayQuery)) {}

		float shadow = 1.0;
		if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
			float occularDistance = rayQueryGetIntersectionTEXT(rayQuery, true) ;
			float penumbra = clamp(occularDistance / 3, 0.0, 1.0);
			shadow = 1.0 - penumbra;
		}

		float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
		vec4 ks = lights.specular[i];
		vec4 kd = lights.diffuse[i];

		// Extract brightness multiplier from the w component of diffuse
		float brightness = lights.diffuse[i].w;

		vec3 reflection = normalize(reflect(-lightDirection, normal));
		float spec = pow(max(dot(eyeDirection, reflection), 0.0), 14.0);

		// Scale diffuse and specular lighting by brightness (shadows scale proportionally, staying dark)
		phongResult += (((diff * kd * kt * attenuation) + (spec * ks * attenuation)) * shadow) * brightness;
	}

	fragColor = phongResult + (ka * kt);
}