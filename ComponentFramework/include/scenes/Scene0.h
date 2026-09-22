#ifndef SCENE0_H
#define SCENE0_H
#include "scenes/Scene.h"
#include "Vector.h"
#include "render/Renderer.h"
//#include "Camera.h"
#include "core/CoreStructs.h"
#include <core/Actor.h>
#include <unordered_map>
#include <Assets/XMLAssetManager.h>

#include "core/CameraActor.h"
using namespace MATH;

/// Forward declarations 
union SDL_Event;


class Scene0 : public Scene {
private:
	
	Renderer *renderer;
	//Camera *camera;
	Matrix4 mariosModelMatrix;
	Matrix4 SkullModelMatrix;
	Sampler2D  mariosPants;
	IndexedVertexBuffer mariosMesh;
	
	Sampler2D  SkullTexture;
	IndexedVertexBuffer SkullMesh;

	std::vector<BufferMemory> cameraUBO;
	CameraData camera;
	std::vector<BufferMemory> lightsUBO;
	LightsData lights;


	DescriptorSetInfo mariosdescriptorSetInfo;
	DescriptorSetInfo SkulldescriptorSetInfo;
	
	DescriptorSetInfo CameraUBOinfo;

	PipelineInfo pipelineInfo;
	CommandBufferData commandBufferData;
	
	Ref<Actor> Mario;
	Ref<Actor> Mario_Mime;

	std::unique_ptr<CameraActor> camera_actor_;
	
	std::unordered_map<std::string, Ref<Actor>> ActorList;
	Ref<XMLAssetManager> assetManager;

public:

	explicit Scene0(Renderer* renderer_);
	virtual ~Scene0();

	virtual bool OnCreate() override;
	virtual void OnDestroy() override;
	virtual void Update(const float deltaTime) override;
	virtual void Render() const override;
	virtual void HandleEvents(const SDL_Event &sdlEvent) override;
};


#endif // SCENE0_H