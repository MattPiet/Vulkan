#include <glew.h>
#include <iostream>
#include "core/Debug.h"
#include "scenes/Scene1.h"
#include <MMath.h>
#include <__msvc_ranges_to.hpp>

#include "physics/TransformComponent.h"
#include "render/VulkanRenderer.h"
#include "render/OpenGLRenderer.h"
#include "scenes/SceneManager.h"
#include <core/Profiling.h>

Scene1::Scene1(Renderer *renderer_): 
	Scene(nullptr),renderer(renderer_) {
	Debug::Info("Created Scene1: ", __FILE__, __LINE__);
}

Scene1::~Scene1() {
}

bool Scene1::OnCreate() {
	MonitorTimingStart("Scene1 OnCreate");
	int width = 0, height = 0;
	float aspectRatio;

	switch (renderer->getRendererType()){
	case RendererType::VULKAN:
	{
		VulkanRenderer* vRenderer;
		vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
			
			assetManager = std::make_shared<XMLAssetManager>();
			std::vector<std::string> names{ 
				"MarioFire",
				"MarioMime",
			};
			std::lock_guard<std::mutex> lock(vulkanMutex);
			camera_actor_ = std::dynamic_pointer_cast<CameraActor>(assetManager->xmlAssets.find("Camera")->second);
			camera_actor_->OnCreate(vRenderer);
			
			lightsManager = std::make_shared<LightsManager>(std::weak_ptr<Component>());
			lightsManager->AddComponent(std::make_shared<DescriptorSetInfo>());
			lightsManager->OnCreate(vRenderer);
			
			bool everythingLoaded = true;
			for (const std::string& name : names) {
				auto iterator = assetManager->xmlAssets.find(name);
				// Is a name missing or has a typo?
				if (iterator == assetManager->xmlAssets.end()) {
					// Strings are handy to use the "+" symbol to join them up
					Debug::Error("Actor not found in Scene1.xml: " + name, __FILE__, __LINE__);
					everythingLoaded = false;
					continue; // skip to the next iteration
				}
				ActorList[name] = std::dynamic_pointer_cast<Actor>(iterator->second);
				ActorList[name]->OnCreate(vRenderer);
			}
			// this makes one pipeline for everything 
			std::vector<VkDescriptorSetLayout> pipelineLayouts = {
				camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSetLayout, 
				lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSetLayout,
				ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
			};
			pipelineInfo = vRenderer->CreateGraphicsPipeline(pipelineLayouts, "shaders/multiPhong.vert.spv", "shaders/multiPhong.frag.spv");
			
			scene_number = 0;
		}
		break;

	case RendererType::OPENGL: 
		break;
	}
	MonitorTimingEnd("Scene1 OnCreate");
	return true;
}

void Scene1::HandleEvents(const SDL_Event& sdlEvent) {
	camera_actor_->SetQuat(sdlEvent);
		switch (sdlEvent.type) {
			
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			printf("size changed %d %d\n", sdlEvent.window.data1, sdlEvent.window.data2);
			float aspectRatio = static_cast<float>(sdlEvent.window.data1) / static_cast<float>(sdlEvent.window.data2);
			camera_actor_->UpdateProjectionMatrix(45.0f, aspectRatio, 0.5f, 100.0f);
			if (renderer->getRendererType() == RendererType::VULKAN)
			{
				VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
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
					"shaders/multiPhong.vert.spv", 
					"shaders/multiPhong.frag.spv"
				);
				camera_actor_->CreateSkyboxPipeline(vRenderer);
				
			}
			break;
		}
	
}
void Scene1::Update(const float deltaTime) {
	
	if (renderer->getRendererType() == RendererType::VULKAN)
	{
		VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
		camera_actor_->CameraMovement(deltaTime, nullptr);
		vRenderer->UpdateUniformBuffer<CameraData>(camera_actor_->GetCameraData(), camera_actor_->GetCameraUBO());

		LightsData viewSpaceLights = lightsManager->GetLightsData(); 
		for (int i = 0; i < viewSpaceLights.numLights; i++)
		{
			Vec4 worldPosition = viewSpaceLights.pos[i]; 
			viewSpaceLights.pos[i] = camera_actor_->GetViewMatrix() * worldPosition; 
		}
		vRenderer->UpdateUniformBuffer<LightsData>(viewSpaceLights, lightsManager->GetLightsUBO());
		
	}
	elapsedTime += deltaTime;
	ActorList.at("MarioFire")->GetComponent<TransformComponent>()->SetOrientation
	(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(0.0f, 1.0f, 0.0f))) ;
	ActorList.at("MarioMime")->GetComponent<TransformComponent>()->SetOrientation
	(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(1.0f, 0.0f, 0.0f))) ;
	
	if (elapsedTime >= 5.0f && !hasLoaded)
	{
		scene_number = 0;
		loadStagedScene = true;
		hasLoaded = true; // Lock it out from firing again
	}
}

void Scene1::Render() const {
		switch (renderer->getRendererType())
		{
		case RendererType::VULKAN:
			{
				VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
				std::lock_guard<std::mutex> lock(vulkanMutex);
				vRenderer->RecordCommandBuffers(Recording::START);
				
				vRenderer->BindPipeline(camera_actor_->GetSkyboxPipelineInfo().pipeline);
				vRenderer->BindDescriptorSet(camera_actor_->GetSkyboxPipelineInfo().pipelineLayout, 0, camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSet); // Set 0: Camera UBO
				vRenderer->BindDescriptorSet(camera_actor_->GetSkyboxPipelineInfo().pipelineLayout, 1, camera_actor_->GetComponent<DescriptorSetInfo>()->descriptorSet); // Set 1: Cubemap Texture
				vRenderer->BindMesh(*camera_actor_->GetComponent<IndexedVertexBuffer>());
				vRenderer->DrawIndexed(*camera_actor_->GetComponent<IndexedVertexBuffer>());
				
				// Bind pipeline
				vRenderer->BindPipeline(pipelineInfo.pipeline);
				// bind Camera Once
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 0, camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSet);
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 1, lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSet);
				
				for (const auto& [name, actor] : ActorList) {
					switch (camera_actor_->CheckFrustum(actor->GetModelMatrix()) ) { 
					case FrustumCollision::CULLED_NEAR:
						std::cout << name << ": Clipped through Near Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_FAR:
						std::cout << name << ": Clipped through Far Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_RIGHT:
						std::cout << name << ": Clipped through Right Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_LEFT:
						std::cout << name << ": Clipped through Left Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_TOP:
						std::cout << name << ": Clipped through Top Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_BOTTOM:
						std::cout << name << ": Clipped through Bottom Plane" << std::endl;
						break; 
					default:
						// bind mr mario
						vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 2, actor->GetComponent<DescriptorSetInfo>()->descriptorSet); 
						// set push constant
						vRenderer->SetPushConstant(pipelineInfo, actor->GetModelMatrix());
						// bind his mesh
						vRenderer->BindMesh(*actor->GetComponent<IndexedVertexBuffer>());
						// give him pants
						vRenderer->DrawIndexed(*actor->GetComponent<IndexedVertexBuffer>());
						break;
					}
				}
				// draw
				vRenderer->RecordCommandBuffers(Recording::STOP);
				vRenderer->Render();
				break;
			}

	case RendererType::OPENGL:
		OpenGLRenderer* glRenderer;
		glRenderer = dynamic_cast<OpenGLRenderer*>(renderer);
		/// Clear the screen
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		/// Draw your scene here
		glUseProgram(0);
		
		break;
	}
}


void Scene1::OnDestroy() {
VulkanRenderer* vRenderer;
    vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    if(vRenderer){
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
    }
}
