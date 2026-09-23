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


Scene1::Scene1(Renderer *renderer_): 
	Scene(nullptr),renderer(renderer_) {
	Debug::Info("Created Scene1: ", __FILE__, __LINE__);
}

Scene1::~Scene1() {
}

bool Scene1::OnCreate() {
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
				"MarioMime"
			};
			
		camera_actor_ = std::make_unique<CameraActor>(std::weak_ptr<Component>(), 45.0f, 16.0f / 9.0f, 0.5f, 400.0f);
			camera_actor_->AddComponent(Ref<TransformComponent>() = std::make_shared<TransformComponent>(std::weak_ptr<Component>()));
			camera_actor_->SetView(Quaternion(), Vec3(0.0f,0.0f,5.0f));
		
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(vRenderer->getPhysicalDevice(), &deviceProperties);

			uint32_t maxPushConstantSize = deviceProperties.limits.maxPushConstantsSize;
			
			std::cout << "Max Push Constant Size: " << maxPushConstantSize << " bytes" << std::endl;
			
		
			std::lock_guard<std::mutex> lock(vulkanMutex);
		lightsUBO = vRenderer->CreateUniformBuffers<LightsData>();
		cameraUBO = vRenderer->CreateUniformBuffers<CameraData>();

		SDL_GetWindowSize(vRenderer->getWindow(), &width, &height);
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		camera.projectionMatrix = MMath::perspective(45.0f, aspectRatio, 0.5f, 100.0f);
		camera.projectionMatrix[5] *= -1.0f;
		camera.viewMatrix = MMath::translate(0.0f, 0.0f, -5.0f);

			camera.projectionMatrix = camera_actor_->GetProjectionMatrix();
			camera.viewMatrix = camera_actor_->GetViewMatrix();
		
			// 0
		lights.diffuse[0] = Vec4(0.0, 0.0, 0.9, 0.0);
		lights.specular[0] = Vec4(0.0, 0.0, 0.3, 0.0);
		lights.ambient = Vec4(1.01, 1.01, 1.01, 0.0);
		lights.numLights = 4;
		lights.pos[0] = Vec4(-4.0f, -5.0f, -5.0f, 0.0f);
			// 1
		lights.diffuse[1] = Vec4(0.0, 0.0, 0.9, 0.0);
		lights.specular[1] = Vec4(0.0, 0.0, 0.3, 0.0);
		lights.pos[1] = Vec4(-4.0f, 5.0f, -5.0f, 0.0f);
			// 2
		lights.diffuse[2] = Vec4(0.9, 0.0, 0.0, 0.0);
		lights.specular[2] = Vec4(0.3, 0.0, 0.0, 0.0);
		lights.pos[2] = Vec4(4.0f, 5.0f, -5.0f, 0.0f);
			// 2
		lights.diffuse[3] = Vec4(0.9, 0.0, 0.0, 0.0);
		lights.specular[3] = Vec4(0.3, 0.0, 0.0, 0.0);
		lights.pos[3] = Vec4(4.0f, -5.0f, -5.0f, 0.0f);
			
			//Matrix4 identity;
			for (int i = 0; i < lights.numLights; i++)
			{
				Vec3 localPosistion = lights.pos[i];
				Vec3 worldPosistion = mariosModelMatrix * localPosistion;
				lights.pos[i] = camera.viewMatrix * worldPosistion;
			}
			
		vRenderer->UpdateUniformBuffer<LightsData>(lights, lightsUBO);
		vRenderer->UpdateUniformBuffer<CameraData>(camera, cameraUBO);
			
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
			}
		DescriptorSetBuilder descriptor_set_builder(vRenderer->getDevice());
		descriptor_set_builder.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, cameraUBO);
		descriptor_set_builder.add(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, lightsUBO);
		CameraUBOinfo = descriptor_set_builder.BuildDescriptorSet(vRenderer->getNumSwapchains());
					
			for (const auto& pair : ActorList) {
			    Ref<Actor> actor = pair.second;
			    if (actor == nullptr ||
			       actor->GetComponent<DescriptorSetInfo>()   == nullptr ||
			       actor->GetComponent<Sampler2D>() == nullptr ||
			       actor->GetComponent<IndexedVertexBuffer>()     == nullptr)
			    {
			       Debug::Error("Actor is missing a shader, material, mesh or shape: " + pair.first, __FILE__, __LINE__);
			       everythingLoaded = false;
			    }
			    else
			    {
			          *actor->GetComponent<Sampler2D>() =  vRenderer->Create2DTextureImage(actor->GetComponent<Sampler2D>()->filename.c_str());
			          *actor->GetComponent<IndexedVertexBuffer>() = vRenderer->LoadModelIndexed(actor->GetComponent<IndexedVertexBuffer>()->filename.c_str());
			          
			          std::string vertName = actor->GetComponent<DescriptorSetInfo>()->VertFilename;
			          std::string fragName = actor->GetComponent<DescriptorSetInfo>()->FragFilename;
			          {
			             DescriptorSetBuilder actorDescriptorBuilder(vRenderer->getDevice());
			             actorDescriptorBuilder.add(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, actor->GetComponent<Sampler2D>().get());
			             *actor->GetComponent<DescriptorSetInfo>() = actorDescriptorBuilder.BuildDescriptorSet(vRenderer->getNumSwapchains());
			          }
			    	// create graphics pipeline for every actor, might be useful if two actors have different layouts. NOT MESHS OR TEXTURES LAYOUTS 
			    	// which is highly unlikely
			       /*std::vector<VkDescriptorSetLayout> pipelineLayouts = {
			           CameraUBOinfo.descriptorSetLayout, 
			           actor->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
			       };
			    
			       pipelineInfo = vRenderer->CreateGraphicsPipeline(pipelineLayouts, vertName.c_str(), fragName.c_str());*/
			    }
			}
			// this makes one pipeline for everything 
			std::vector<VkDescriptorSetLayout> pipelineLayouts = {
				CameraUBOinfo.descriptorSetLayout, 
				ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
			};
			pipelineInfo = vRenderer->CreateGraphicsPipeline(pipelineLayouts, "shaders/multiPhong.vert.spv", "shaders/multiPhong.frag.spv");
		
	
		}
		break;

	case RendererType::OPENGL: 
		break;
	}
	std::cout << "Scene 1 fully loaded" << std::endl;
	return true;
}

void Scene1::HandleEvents(const SDL_Event& sdlEvent) {
	camera_actor_->SetQuat(sdlEvent);
		switch (sdlEvent.type) {
			
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			printf("size changed %d %d\n", sdlEvent.window.data1, sdlEvent.window.data2);
			float aspectRatio = static_cast<float>(sdlEvent.window.data1) / static_cast<float>(sdlEvent.window.data2);
			///camera->Perspective(45.0f, aspectRatio, 0.5f, 20.0f);
			if(renderer->getRendererType() == RendererType::VULKAN){
				dynamic_cast<VulkanRenderer*>(renderer)->RecreateSwapChain();
			}
			break;
		}
	
}
void Scene1::Update(const float deltaTime) {
	// Apply the Vulkan Y-flip if using Vulkan
	if (renderer->getRendererType() == RendererType::VULKAN)
	{
	
		VulkanRenderer* vRenderer;
		vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
	
		camera_actor_->CameraMovement(deltaTime, nullptr);
		camera.projectionMatrix = camera_actor_->GetProjectionMatrix();
		camera.projectionMatrix[5] *= -1.0f;
		camera.viewMatrix = camera_actor_->GetViewMatrix();
		vRenderer->UpdateUniformBuffer<CameraData>(camera, cameraUBO);
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

	if (elapsedTime >= 10.0f && !hasSwapped)
	{
		swapscene = true;
		hasSwapped = true; 
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
				// build pipeline
				vRenderer->BindPipeline(pipelineInfo.pipeline);
				
				// bind Camera Once
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 0, CameraUBOinfo.descriptorSet);
				
				for (const auto& [name, actor] : ActorList) {
					//bind mr mario
					vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 1, actor->GetComponent<DescriptorSetInfo>()->descriptorSet); 
					// set push constant
					vRenderer->SetPushConstant(pipelineInfo, actor->GetModelMatrix());
					//bind his mesh
					vRenderer->BindMesh(*actor->GetComponent<IndexedVertexBuffer>());
					// give him pants
					vRenderer->DrawIndexed(*actor->GetComponent<IndexedVertexBuffer>());
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
		vRenderer->DestroyUBO(lightsUBO);
		vRenderer->DestroyUBO(cameraUBO);
		vRenderer->DestroyDescriptorSet(CameraUBOinfo);
		for (const auto& [name, actor] : ActorList)
		{
			vRenderer->DestroyDescriptorSet(*actor->GetComponent<DescriptorSetInfo>());
			vRenderer->DestroySampler2D(*actor->GetComponent<Sampler2D>());
			vRenderer->DestroyIndexedMesh(*actor->GetComponent<IndexedVertexBuffer>());
			
		}
		ActorList.clear();
		assetManager.reset();
	}
}
