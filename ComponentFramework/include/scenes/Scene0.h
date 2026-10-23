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
#include <core/ML.h>

#include "core/LightsManager.h"
using namespace MATH;

/// Forward declarations 
union SDL_Event;


class Scene0 : public Scene {
private:
	
	Renderer *renderer;
	//Camera *camera;	
	std::shared_ptr<LightsManager> lightsManager;
	
	PipelineInfo pipelineInfo;
	CommandBufferData commandBufferData;
	std::shared_ptr<CameraActor> camera_actor_;
	std::unordered_map<std::string, Ref<Actor>> ActorList;
	// Previous model matrices from the last rendered frame, used for DLSS motion vectors.
	mutable std::unordered_map<std::string, Matrix4> previousModelMatrices_;
	Ref<XMLAssetManager> assetManager;
	
	float elapsedTime = 0.0f;
	bool hasLoaded = false;
	bool hasSwapped = false;
	
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