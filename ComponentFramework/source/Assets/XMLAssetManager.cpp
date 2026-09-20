#include "Assets/XMLAssetManager.h"
#include <iostream>
#include <fstream>
#include <string>
#include <QMath.h>
#include "Core/Debug.h"
#include "physics/TransformComponent.h"
#include <assert.h>

#include "core/CoreStructs.h"

XMLAssetManager::XMLAssetManager()
{

	tinyxml2::XMLDocument doc;
	doc.LoadFile("XMLFiles/Scene0.xml");

	// Get the top of the node system
	tinyxml2::XMLElement* rootData = doc.RootElement();

	// Everything we load lives under <Scene0>.
	tinyxml2::XMLElement* sceneData = rootData->FirstChildElement("Scene0");

	for (const tinyxml2::XMLElement* child = rootData->FirstChildElement("Scene0")->FirstChildElement();
		child;
		child = child->NextSiblingElement())
	{
		AddMaterial(child);
		AddShader(child);
	//	AddCamera(child);
	//	AddLight(child);
		
		if (std::string(child->Name()) == "Mesh") {
			// Get mesh name and filename
			const char* meshName = child->Attribute("name");
			const char* meshFilename = child->Attribute("filename");
			AddComponent<IndexedVertexBuffer>(meshName, std::shared_ptr<IndexedVertexBuffer>() = std::make_shared<IndexedVertexBuffer>());
			GetComponent<IndexedVertexBuffer>(meshName)->filename = meshFilename;
		}
	}

	// Now we have all the shared assets ready, time to build the actors
	for (const tinyxml2::XMLElement* child = rootData->FirstChildElement("Scene0")->FirstChildElement("Actor");
		child;
		child = child->NextSiblingElement())
	{
		if (std::string(child->Name()) == "Actor") {
			const char* actorName = child->Attribute("actorname");
			const char* parentName = child->Attribute("parent");
			std::weak_ptr<Component> parent = std::weak_ptr<Component>();
			if (parentName != nullptr && std::string(parentName) != "none") {
				parent = GetComponent<Actor>(parentName);
			}

			Ref<Actor> actor = std::make_shared<Actor>(parent);
			// Add shared assets to the actor
			AddMeshToActor(child, actor);
			AddShaderToActor(child, actor);
			AddMaterialToActor(child, actor);
		//	AddShapeToActor(child, actor);
			// The transform is unique for the actor. Needs the parent too
			AddTransformToActor(child, actor, parent);
			// Add physics to the actor AFTER the transform. We need them to match in position and orientation
		//	AddPhysicsToActor(child, actor, parent);

			// Use OnCreate for the actor (which in turn fires OnCreate for each component).
			if (actor->OnCreate() == false) {
				Debug::Error(std::string("Actor ") + actorName + " could not load all of its assets", __FILE__, __LINE__);
			}
			AddComponent(child->Attribute("actorname"), actor);
		}
	}
}
XMLAssetManager::~XMLAssetManager()
{
	xmlAssets.clear();
}

MATH::Vec3 XMLAssetManager::ReadVec3(const tinyxml2::XMLElement* element, const char* xName, const char* yName, const char* zName) {
	// Make a vector from the three floats in the xml file
	return Vec3(
		element->FloatAttribute(xName),
		element->FloatAttribute(yName),
		element->FloatAttribute(zName)
	);
}

void XMLAssetManager::AddMaterial(const tinyxml2::XMLElement* child)
{
	if (std::string(child->Name()) == "Material") {
		const char* name = child->Attribute("name");
		const char* filename = child->Attribute("filename");
		AddComponent<Sampler2D>(name, std::shared_ptr<Sampler2D>() = std::make_shared<Sampler2D>());
		GetComponent<Sampler2D>(name)->filename = filename;
	}
}

void XMLAssetManager::AddShader(const tinyxml2::XMLElement* child)
{
	if (std::string(child->Name()) == "Shader") {
		const char* name = child->Attribute("name");
		const char* vertFilename = child->Attribute("vertFilename");
		const char* fragFilename = child->Attribute("fragFilename");
		AddComponent<DescriptorSetInfo>(name ,std::shared_ptr<DescriptorSetInfo>() = std::make_shared<DescriptorSetInfo>());
		GetComponent<DescriptorSetInfo>(name)->VertFilename = vertFilename;
		GetComponent<DescriptorSetInfo>(name)->FragFilename = fragFilename;
	}
}

/*
void XMLAssetManager::AddCamera(const tinyxml2::XMLElement* child)
{
	if (std::string(child->Name()) != "Camera") return;

	const char* cameraName = child->Attribute("cameraname");
	// Grab the <Transform> once, then read the numbers off it.
	const tinyxml2::XMLElement* transform = child->FirstChildElement("Transform");
	// If we got this far, time to load in the data
	Vec3 axis = ReadVec3(transform, "axisx", "axisy", "axisz");
	Vec3 cameraPos = ReadVec3(transform, "posx", "posy", "posz");
	float angleDeg = transform->FloatAttribute("angleDeg");
	Component* cameraParent = nullptr;

	Ref<CameraActor> camera = std::make_shared<CameraActor>(cameraParent);
	camera->AddComponent<TransformComponent>(nullptr, cameraPos, QMath::angleAxisRotation(angleDeg, axis));
	camera->OnCreate();
	AddComponent(cameraName, camera);
}
*/

/*void XMLAssetManager::AddLight(const tinyxml2::XMLElement* child)
{
	if (std::string(child->Name()) != "Light") return;

	const char* lightName = child->Attribute("lightname");

	// Build Light
	Vec3 lightPos = ReadVec3(child, "posx", "posy", "posz");
	Vec4 colour = ReadVec3(child, "red", "green", "blue");
	colour.w = child->FloatAttribute("alpha");

	float intensity = child->FloatAttribute("intensity");

	Vec3 falloff = ReadVec3(child, "falloffx", "falloffy", "falloffz");

	// We've only coded a DirectionLight so far. Others will have to be next time!
	// const char* style = child->Attribute("lightstyle");
	LightStyle lightstyle = LightStyle::DirectionLight;

	Component* lightParent = nullptr;
	Ref<LightActor> light = std::make_shared<LightActor>(lightParent, lightstyle, lightPos, colour, intensity, falloff);
	light->OnCreate();
	AddComponent(lightName, light);
}*/

void XMLAssetManager::AddMeshToActor(const tinyxml2::XMLElement* child, Ref<Actor> actor)
{
	if (child->FirstChildElement("Mesh")) {
		const char* name = child->FirstChildElement("Mesh")->Attribute("name");
		Ref<IndexedVertexBuffer> mesh = GetComponent<IndexedVertexBuffer>(name);
		actor->AddComponent<IndexedVertexBuffer>(mesh);		
	}

}

void XMLAssetManager::AddShaderToActor(const tinyxml2::XMLElement* child, Ref<Actor> actor)
{
	if (child->FirstChildElement("Shader")) {
		const char* name = child->FirstChildElement("Shader")->Attribute("name");
		Ref<DescriptorSetInfo> sharedShader = GetComponent<DescriptorSetInfo>(name);

		Ref<DescriptorSetInfo> uniqueShader = std::make_shared<DescriptorSetInfo>(*sharedShader);
		actor->AddComponent<DescriptorSetInfo>(uniqueShader);
	}
}

void XMLAssetManager::AddMaterialToActor(const tinyxml2::XMLElement* child, Ref<Actor> actor)
{
	if (child->FirstChildElement("Material")) {
		const char* name = child->FirstChildElement("Material")->Attribute("name");
		Ref<Sampler2D> material = GetComponent<Sampler2D>(name);
		actor->AddComponent<Sampler2D>(material);
	}
}

/*
void XMLAssetManager::AddShapeToActor(const tinyxml2::XMLElement* child, Ref<Actor> actor)
{
	if (std::string(child->FirstChildElement("Shape")->Name()) == "Shape") {
		const char* name = child->FirstChildElement("Shape")->Attribute("name");
		Ref<ShapeComponent> shape = GetComponent<ShapeComponent>(name);
		actor->AddComponent<ShapeComponent>(shape);
	}
}
*/

void XMLAssetManager::AddTransformToActor(const tinyxml2::XMLElement* child, Ref<Actor> actor, std::weak_ptr<Component> parent)
{
	const tinyxml2::XMLElement* element = child->FirstChildElement("Transform");
	if (element == nullptr) return;

	// If we got this far, time to load in data
	Vec3 axis  = ReadVec3(element, "axisx", "axisy", "axisz");
	Vec3 pos   = ReadVec3(element, "posx", "posy", "posz");
	Vec3 scale = ReadVec3(element, "scalex", "scaley", "scalez");
	float angleDeg = child->FirstChildElement("Transform")->FloatAttribute("angleDeg");;
		
	actor->AddComponent<TransformComponent>(parent, pos, QMath::angleAxisRotation(angleDeg, axis), scale);
}

/*
void XMLAssetManager::AddPhysicsToActor(const tinyxml2::XMLElement* child, Ref<Actor> actor, std::weak_ptr<Component> parent)
{
	// TODO for assignment 3
	// Add a default physics component to the actor, but don't forget to match transform component's position & orientation
	// You're gonna have to assume the transform component has already been built (fingers crossed!)
}
*/

