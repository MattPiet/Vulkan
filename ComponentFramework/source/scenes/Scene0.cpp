#include <glew.h>
#include <iostream>
#include "core/Debug.h"
#include "scenes/Scene0.h"
#include <MMath.h>

#include "physics/TransformComponent.h"
#include "render/VulkanRenderer.h"
#include "render/OpenGLRenderer.h"


Scene0::Scene0(Renderer *renderer_): 
	Scene(nullptr),renderer(renderer_) {
	Debug::Info("Created Scene0: ", __FILE__, __LINE__);
}

Scene0::~Scene0() {
}

bool Scene0::OnCreate() {
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
			
		
		
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(vRenderer->getPhysicalDevice(), &deviceProperties);

			uint32_t maxPushConstantSize = deviceProperties.limits.maxPushConstantsSize;
			
			std::cout << "Max Push Constant Size: " << maxPushConstantSize << " bytes" << std::endl;
			
		
			
		lightsUBO = vRenderer->CreateUniformBuffers<LightsData>();
		cameraUBO = vRenderer->CreateUniformBuffers<CameraData>();

		SDL_GetWindowSize(vRenderer->getWindow(), &width, &height);
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		camera.projectionMatrix = MMath::perspective(45.0f, aspectRatio, 0.5f, 100.0f);
		camera.projectionMatrix[5] *= -1.0f;
		camera.viewMatrix = MMath::translate(0.0f, 0.0f, -5.0f);
		
			// 0
		lights.diffuse[0] = Vec4(0.0, 0.0, 0.9, 0.0);
		lights.specular[0] = Vec4(0.0, 0.0, 0.3, 0.0);
		lights.ambient = Vec4(0.01, 0.01, 0.01, 0.0);
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
					Debug::Error("Actor not found in Scene0.xml: " + name, __FILE__, __LINE__);
					everythingLoaded = false;
					continue; // skip to the next iteration
				}
				ActorList[name] = std::dynamic_pointer_cast<Actor>(iterator->second);
			}
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
						{
							DescriptorSetBuilder descriptorSetBuilder(vRenderer->getDevice());
							descriptorSetBuilder.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, cameraUBO);
		
							descriptorSetBuilder.add(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, lightsUBO);

							descriptorSetBuilder.add(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, actor->GetComponent<Sampler2D>().get());
							*actor->GetComponent<DescriptorSetInfo>() = descriptorSetBuilder.BuildDescriptorSet(vRenderer->getNumSwapchains());
						}
					pipelineInfo = vRenderer->CreateGraphicsPipeline(actor->GetComponent<DescriptorSetInfo>()->descriptorSetLayout,
						"shaders/multiPhong.vert.spv", "shaders/multiPhong.frag.spv");
				}
			}
		
	
	}
		break;

	case RendererType::OPENGL:
		break;
	}

	return true;
}

void Scene0::HandleEvents(const SDL_Event& sdlEvent) {
	
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
void Scene0::Update(const float deltaTime) {
	static float elapsedTime = 0.0f;
	elapsedTime += deltaTime;
	ActorList.at("MarioFire")->GetComponent<TransformComponent>()->SetOrientation(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(0.0f, 1.0f, 0.0f))) ;
	ActorList.at("MarioMime")->GetComponent<TransformComponent>()->SetOrientation(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(1.0f, 0.0f, 0.0f))) ;
}

void Scene0::Render() const {
		switch (renderer->getRendererType()) {

	case RendererType::VULKAN:
		VulkanRenderer* vRenderer;
		vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
		vRenderer->RecordCommandBuffers(Recording::START);
			for (const auto& [name,actor] : ActorList)
			{
				vRenderer->BindMesh(*actor->GetComponent<IndexedVertexBuffer>());
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, actor->GetComponent<DescriptorSetInfo>()->descriptorSet);
				vRenderer->BindPipeline(pipelineInfo.pipeline);
				vRenderer->SetPushConstant(pipelineInfo, actor->GetModelMatrix());
				vRenderer->DrawIndexed(*actor->GetComponent<IndexedVertexBuffer>());
			}
			vRenderer->RecordCommandBuffers(Recording::STOP);
			vRenderer->Render();
		break;

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


void Scene0::OnDestroy() {
	VulkanRenderer* vRenderer;
	vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
	if(vRenderer){
		vkDeviceWaitIdle(vRenderer->getDevice());
		vRenderer->DestroyCommandBuffers();
		vRenderer->DestroyPipeline(pipelineInfo);
		vRenderer->DestroyDescriptorSet(mariosdescriptorSetInfo);
		vRenderer->DestroyUBO(lightsUBO);
		vRenderer->DestroyUBO(cameraUBO);
		
		vRenderer->DestroySampler2D(mariosPants);
		vRenderer->DestroyIndexedMesh(mariosMesh);
		}
}
