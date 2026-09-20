#version 450
#extension GL_ARB_separate_shader_objects : enable
#define MAX_LIGHTS 4

layout (location = 0) in  vec4 vVertex;
layout (location = 1) in  vec4 vNormal;
layout (location = 2) in  vec2 texCoords;

layout(binding = 0) uniform CameraUBO {
    mat4 projectionMatrix;
	mat4 viewMatrix;
} camera;

layout(push_constant) uniform Push {
	mat4 modelMatrix;
	mat3x4 normalMatrix; 
	vec4 pad;
} push;

layout(binding = 1) uniform LightsUBO {
 vec4 pos[MAX_LIGHTS];
 vec4 diffuse[MAX_LIGHTS];
 vec4 specular[MAX_LIGHTS];
 vec4 ambient;
 uint numLights;
} lights;

layout (location = 0) out vec3 vertNormal;
layout (location = 1) out vec3 eyeDir;
layout (location = 2) out vec2 fragTexCoords;
layout (location = 3) out vec3 lightDir[MAX_LIGHTS];


void main() {
	fragTexCoords = texCoords;
	mat3 normalMatrix = mat3(transpose(inverse(push.modelMatrix)));

	vertNormal = normalize(normalMatrix * vNormal.xyz); /// Rotate the normal to the correct orientation 
	vec3 vertPos = vec3(camera.viewMatrix * push.modelMatrix * vVertex); /// This is the position of the vertex from the origin
	vec3 vertDir = normalize(vertPos);
	eyeDir = -vertDir;
	/// Light position from the point-of-view of each vertex
	vec3 lightLocFromVertex[MAX_LIGHTS];
	for(int i =0; i < lights.numLights; i++){
		lightLocFromVertex[i] = vec3(lights.pos[i]) - vertPos; 
		lightDir[i] = normalize(lightLocFromVertex[i]); /// Create the light direction.
	}
	
	gl_Position =  camera.projectionMatrix * camera.viewMatrix * push.modelMatrix * vVertex; 
}
