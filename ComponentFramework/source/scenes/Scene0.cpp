#include <glew.h>
#include <iostream>
#include "core/Debug.h"
#include "scenes/Scene0.h"
#include <MMath.h>
#include <__msvc_ranges_to.hpp>

#include "physics/TransformComponent.h"
#include "render/VulkanRenderer.h"
#include "render/OpenGLRenderer.h"
#include "scenes/SceneManager.h"
#include <core/Profiling.h>

Scene0::Scene0(Renderer *renderer_): 
	Scene(nullptr),renderer(renderer_) {
	Debug::Info("Created Scene0: ", __FILE__, __LINE__);
}

Scene0::~Scene0() {
}

bool Scene0::OnCreate() {
	previousModelMatrices_.clear();
	MonitorTimingStart("Scene0 OnCreate");
	int width = 0, height = 0;
	float aspectRatio;

	switch (renderer->getRendererType()){
	case RendererType::VULKAN:
	{
		VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
		vRenderer->ResetDLSSMotionHistory();
			
			assetManager = std::make_shared<XMLAssetManager>();
			std::vector<std::string> names{ 
				"MarioFire",
				"MarioMime",
				"Rook"
			};
			
			std::scoped_lock lock(vulkanMutex);
			camera_actor_ = std::dynamic_pointer_cast<CameraActor>(assetManager->xmlAssets.find("Camera")->second);
			camera_actor_->OnCreate(vRenderer);
			if (vRenderer->IsDLSSActive()) {
				// Initialize the camera data used by the DLSS motion-vector pass.
				camera_actor_->UpdateViewMatrix();
				vRenderer->UpdateUniformBuffer<CameraData>(
					camera_actor_->GetCameraData(), camera_actor_->GetCameraUBO());
			}
			
			lightsManager = std::make_shared<LightsManager>(std::weak_ptr<Component>());
			lightsManager->AddComponent(std::make_shared<DescriptorSetInfo>());
			lightsManager->OnCreate(vRenderer);
			lightsManager->renderMeshes = true;
			
			for (const std::string& name : names) {
				auto iterator = assetManager->xmlAssets.find(name);
				if (iterator == assetManager->xmlAssets.end()) {
					Debug::Error("Actor not found in Scene0.xml: " + name, __FILE__, __LINE__);
					continue; 
				}
				ActorList[name] = std::dynamic_pointer_cast<Actor>(iterator->second);
				ActorList[name]->OnCreate(vRenderer);
			}
			// this makes one pipeline for everything (I'm a dirty liar the skybox has its own pipeline ;)
			std::vector<VkDescriptorSetLayout> pipelineLayouts = {
				camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSetLayout, 
				lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSetLayout,
				ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
			};
			pipelineInfo = vRenderer->CreateGraphicsPipeline(
				pipelineLayouts,
				vRenderer->IsDLSSActive() ? "shaders/multiPhong_dlss.vert.spv" : "shaders/multiPhong.vert.spv",
				vRenderer->IsDLSSActive() ? "shaders/multiPhong_dlss.frag.spv" : "shaders/multiPhong.frag.spv");
			
			scene_number = 1;
		}
		break;

	case RendererType::OPENGL: 
		break;
	}
	MonitorTimingEnd("Scene0 OnCreate");
	return true;
}

void Scene0::HandleEvents(const SDL_Event& sdlEvent) {
	if (camera_actor_) camera_actor_->SetQuat(sdlEvent);
		switch (sdlEvent.type) {
			
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			printf("size changed %d %d\n", sdlEvent.window.data1, sdlEvent.window.data2);
			float aspectRatio = static_cast<float>(sdlEvent.window.data1) / static_cast<float>(sdlEvent.window.data2);
			camera_actor_->UpdateProjectionMatrix(45.0f, aspectRatio, 0.5f, 100.0f);
			if (renderer->getRendererType() == RendererType::VULKAN)
			{
				VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
				previousModelMatrices_.clear();
				lightsManager->ResetMotionHistory();
			vRenderer->ResetDLSSMotionHistory();
				if (vRenderer->IsDLSSActive()) {
					vRenderer->UpdateUniformBuffer<CameraData>(
						camera_actor_->GetCameraData(), camera_actor_->GetCameraUBO());
				}
				vRenderer->RecreateSwapChain();
				vRenderer->DestroyPipeline(pipelineInfo);
				vRenderer->DestroyPipeline(camera_actor_->GetSkyboxPipelineInfo());
				std::vector<VkDescriptorSetLayout> pipelineLayouts = {
					camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSetLayout, 
					lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSetLayout,
					ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
				};
				pipelineInfo = vRenderer->CreateGraphicsPipeline(
					pipelineLayouts,
					vRenderer->IsDLSSActive() ? "shaders/multiPhong_dlss.vert.spv" : "shaders/multiPhong.vert.spv",
					vRenderer->IsDLSSActive() ? "shaders/multiPhong_dlss.frag.spv" : "shaders/multiPhong.frag.spv"
				);
				camera_actor_->CreateSkyboxPipeline(vRenderer);
			}
			break;
		}
	}
void Scene0::Update(const float deltaTime) {
	
	if (renderer->getRendererType() == RendererType::VULKAN)
	{
		VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
		camera_actor_->CameraMovement(deltaTime, nullptr);
		vRenderer->UpdateUniformBuffer<CameraData>(camera_actor_->GetCameraData(), camera_actor_->GetCameraUBO());
		
		LightsData dynamicLights = lightsManager->GetLightsData(); 
       
		float pulseSpeed = 1.0f; 
		float minRadius = 6.0f;
		float maxRadius = 50.0f;

		// Use negative cos so the wave starts at the exact minimum value (0.0) at time = 0
		float bounce = (-std::cos(elapsedTime * pulseSpeed) + 1.0f) * 0.5f;

		float radius = minRadius + (maxRadius - minRadius) * bounce;

		float speed = 1.0f;  // Rotation speed in radians per second

		// Light 0
		float x = radius * std::cos(elapsedTime * speed);
		float z = radius * std::sin(elapsedTime * speed);
		float y = lightsManager->getLightTransforms().at("0")->GetPosition().y;
		lightsManager->setPosition("0", Vec3(x, y, z));
       
		// Light 1 (Offset by 90 degrees / 1.57 radians)
		x = radius * std::cos(elapsedTime * speed + 1.57f);
		z = radius * std::sin(elapsedTime * speed + 1.57f);
		y = lightsManager->getLightTransforms().at("1")->GetPosition().y;
		lightsManager->setPosition("1", Vec3(x, y, z));
       
		// Light 2 (Offset by 180 degrees / 3.14 radians)
		x = radius * std::cos(elapsedTime * speed + 3.14f);
		z = radius * std::sin(elapsedTime * speed + 3.14f);
		y = lightsManager->getLightTransforms().at("2")->GetPosition().y;
		lightsManager->setPosition("2", Vec3(x, y, z));
       
		// Light 3 (Offset by 270 degrees / 4.71 radians)
		x = radius * std::cos(elapsedTime * speed + 4.71f);
		z = radius * std::sin(elapsedTime * speed + 4.71f);
		y = lightsManager->getLightTransforms().at("3")->GetPosition().y;
		lightsManager->setPosition("3", Vec3(x, y, z));

	
		vRenderer->UpdateUniformBuffer<LightsData>(lightsManager->GetLightsData(), lightsManager->GetLightsUBO());
		
	}
	elapsedTime += deltaTime;
	ActorList.at("MarioFire")->GetComponent<TransformComponent>()->SetOrientation
	(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(0.0f, 1.0f, 0.0f))) ;
	ActorList.at("MarioMime")->GetComponent<TransformComponent>()->SetOrientation
	(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(1.0f, 0.0f, 0.0f))) ;
	
	/*if (elapsedTime >= 5.0f && !hasLoaded)
	{
		scene_number = 1;
		loadStagedScene = true;
		hasLoaded = true; // Lock it out from firing again
	}*/
}

void Scene0::Render() const {
		switch (renderer->getRendererType())
		{
		case RendererType::VULKAN:
			{
				VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
				std::scoped_lock lock(vulkanMutex);
				if (vRenderer->IsDLSSActive()) {
					vRenderer->SetDLSSCameraMatrices(
						camera_actor_->GetProjectionMatrix(),
						camera_actor_->GetViewMatrix());
					CameraData frameCameraData = camera_actor_->GetCameraData();
					frameCameraData.previousProjectionMatrix = vRenderer->GetPreviousDLSSProjectionForFrame();
					frameCameraData.previousViewMatrix = vRenderer->GetPreviousDLSSViewForFrame();
					vRenderer->UpdateUniformBuffer<CameraData>(
						frameCameraData, camera_actor_->GetCameraUBO());
				}
				vRenderer->RecordCommandBuffers(Recording::START);
				
				camera_actor_->render(vRenderer);
				// Bind pipeline
				vRenderer->BindPipeline(pipelineInfo.pipeline);
				// bind Camera Once
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 0, camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSet);
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 1, lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSet);
				
				for (const auto& [name, actor] : ActorList) {
					const Matrix4 currentModel = actor->GetModelMatrix();
					const auto previousIt = previousModelMatrices_.find(name);
					const Matrix4 previousModel = (previousIt == previousModelMatrices_.end())
						? currentModel : previousIt->second;
					switch (camera_actor_->CheckFrustum(currentModel) ) { 
					case FrustumCollision::CULLED_NEAR:
						std::cout << name << ": Clipped through Near Plane\n";
						break;
					case FrustumCollision::CULLED_FAR:
						std::cout << name << ": Clipped through Far Plane\n";
						break;
					case FrustumCollision::CULLED_RIGHT:
						std::cout << name << ": Clipped through Right Plane\n";
						break;
					case FrustumCollision::CULLED_LEFT:
						std::cout << name << ": Clipped through Left Plane\n";
						break;
					case FrustumCollision::CULLED_TOP:
						std::cout << name << ": Clipped through Top Plane\n";
						break;
					case FrustumCollision::CULLED_BOTTOM:
						std::cout << name << ": Clipped through Bottom Plane\n";
						break; 
					default:
						actor->render(vRenderer, pipelineInfo, &previousModel);
						break;
					}
				}
				// LightsManager renders its meshes with the same multiPhong pipeline.
				// That fragment shader statically samples descriptor set 2. If the camera
				// culls every actor, no actor draw has bound set 2 this frame, so bind a
				// valid actor material before drawing the light meshes.
				if (!ActorList.empty()) {
					auto materialInfo = ActorList.begin()->second->GetComponent<DescriptorSetInfo>();
					if (materialInfo) {
						vRenderer->BindDescriptorSet(
							pipelineInfo.pipelineLayout,
						2,
						materialInfo->descriptorSet);
						lightsManager->render(vRenderer, pipelineInfo);
					}
				}
				for (const auto& [name, actor] : ActorList) {
					previousModelMatrices_[name] = actor->GetModelMatrix();
				}
				// draw
				vRenderer->RecordCommandBuffers(Recording::STOP);
				vRenderer->Render();
				break;
			}

	case RendererType::OPENGL:
		/*OpenGLRenderer* glRenderer;
		glRenderer = dynamic_cast<OpenGLRenderer*>(renderer);
		/// Clear the screen
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		/// Draw your scene here
		glUseProgram(0);
			*/
		break;
	}
}


void Scene0::OnDestroy() {
	previousModelMatrices_.clear();


    if(VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer)){
       vkDeviceWaitIdle(vRenderer->getDevice());

       vRenderer->DestroyPipeline(pipelineInfo);
       vRenderer->DestroyUBO(lightsManager->GetLightsUBO());
       vRenderer->DestroyUBO(camera_actor_->GetCameraUBO());
       vRenderer->DestroyDescriptorSet(camera_actor_->GetCameraUBODescriptorSetInfoRef());
       vRenderer->DestroyDescriptorSet(*lightsManager->GetComponent<DescriptorSetInfo>());
	   vRenderer->DestroyPipeline(camera_actor_->GetSkyboxPipelineInfo());
    	
    	if (camera_actor_ && camera_actor_->GetComponent<DescriptorSetInfo>())  vRenderer->DestroyDescriptorSet(*camera_actor_->GetComponent<DescriptorSetInfo>());
    	if (camera_actor_ && camera_actor_->GetComponent<SamplerCube>())  vRenderer->DestroySamplerCube(*camera_actor_->GetComponent<SamplerCube>());
    	if (camera_actor_ && camera_actor_->GetComponent<IndexedVertexBuffer>()) vRenderer->DestroyIndexedMesh(*camera_actor_->GetComponent<IndexedVertexBuffer>());
       for (const auto& [name, actor] : ActorList)
       {
        if (actor->GetComponent<DescriptorSetInfo>())  vRenderer->DestroyDescriptorSet(*actor->GetComponent<DescriptorSetInfo>());
       	if (actor->GetComponent<Sampler2D>())  vRenderer->DestroySampler2D(*actor->GetComponent<Sampler2D>());
        if (actor->GetComponent<IndexedVertexBuffer>()) vRenderer->DestroyIndexedMesh(*actor->GetComponent<IndexedVertexBuffer>());
       }
       ActorList.clear();
       assetManager.reset();
    	for (auto& [key, mesh] : lightsManager->lightMeshes) {
    		if (mesh) {
    			vRenderer->DestroyIndexedMesh(*mesh);
    		}
    	}
    	for (auto& [key, mesh] : lightsManager->lightMeshes) {
    		delete mesh;
    	}
    	for (auto& [key, transform] : lightsManager->getLightTransforms()) {
    		delete transform;
    	}
    	
    	lightsManager->OnDestroy();
    	lightsManager.reset();
    }
}
