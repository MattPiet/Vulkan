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
    MonitorTimingStart("Scene0 OnCreate");
    int width = 0, height = 0;
    float aspectRatio;

    switch (renderer->getRendererType()){
    case RendererType::VULKAN:
    {
        VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
          
          assetManager = std::make_shared<XMLAssetManager>();
          std::vector<std::string> names{ 
             "MarioFire",
             "MarioMime",
             "Rook"
          };
          
          std::scoped_lock lock(vulkanMutex);
          camera_actor_ = std::dynamic_pointer_cast<CameraActor>(assetManager->xmlAssets.find("Camera")->second);
          camera_actor_->OnCreate(vRenderer);
          
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
          
          std::vector<VkDescriptorSetLayout> pipelineLayouts = {
             camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSetLayout, 
             lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSetLayout,
             ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
          };
          pipelineInfo = vRenderer->CreateGraphicsPipeline(pipelineLayouts, "shaders/multiPhong.vert.spv", "shaders/multiPhong.frag.spv");
          
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

       float bounce = (-std::cos(elapsedTime * pulseSpeed) + 1.0f) * 0.5f;
       float radius = minRadius + (maxRadius - minRadius) * bounce;
       float speed = 1.0f;  

       // Light 0
       float x = radius * std::cos(elapsedTime * speed);
       float z = radius * std::sin(elapsedTime * speed);
       float y = lightsManager->getLightTransforms().at("0")->GetPosition().y;
       lightsManager->setPosition("0", Vec3(x, y, z));
       
       // Light 1 
       x = radius * std::cos(elapsedTime * speed + 1.57f);
       z = radius * std::sin(elapsedTime * speed + 1.57f);
       y = lightsManager->getLightTransforms().at("1")->GetPosition().y;
       lightsManager->setPosition("1", Vec3(x, y, z));
       
       // Light 2 
       x = radius * std::cos(elapsedTime * speed + 3.14f);
       z = radius * std::sin(elapsedTime * speed + 3.14f);
       y = lightsManager->getLightTransforms().at("2")->GetPosition().y;
       lightsManager->setPosition("2", Vec3(x, y, z));
       
       // Light 3 
       x = radius * std::cos(elapsedTime * speed + 4.71f);
       z = radius * std::sin(elapsedTime * speed + 4.71f);
       y = lightsManager->getLightTransforms().at("3")->GetPosition().y;
       lightsManager->setPosition("3", Vec3(x, y, z));

       vRenderer->UpdateUniformBuffer<LightsData>(lightsManager->GetLightsData(), lightsManager->GetLightsUBO());
       
       // --- RAY TRACING: BUILD TLAS EACH FRAME ---
       std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
       uint32_t customIndex = 0;

       for (const auto& [name, actor] : ActorList) {
           Ref<IndexedVertexBuffer> mesh = actor->GetComponent<IndexedVertexBuffer>();
           if (mesh && mesh->blas != VK_NULL_HANDLE) {
               VkAccelerationStructureInstanceKHR instance{};
               instance.transform = vRenderer->Matrix4ToVkTransformMatrixKHR(actor->GetModelMatrix());
               instance.instanceCustomIndex = customIndex++;
               instance.mask = 0xFF;
               instance.instanceShaderBindingTableRecordOffset = 0;
               instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
               instance.accelerationStructureReference = vRenderer->GetAccelerationStructureDeviceAddress(mesh->blas);

               tlasInstances.push_back(instance);
           }
       }

       if (!tlasInstances.empty()) {
           vRenderer->BuildTLAS(
               tlasInstances, 
               tlas, tlasBuffer, tlasMemory, 
               tlasInstanceBuffer, tlasInstanceMemory
           );
       }
       // ------------------------------------------
    }

    elapsedTime += deltaTime;
    ActorList.at("MarioFire")->GetComponent<TransformComponent>()->SetOrientation(
        QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(0.0f, 1.0f, 0.0f))
    );
    ActorList.at("MarioMime")->GetComponent<TransformComponent>()->SetOrientation(
        QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(1.0f, 0.0f, 0.0f))
    );
}

void Scene0::Render() const {
    switch (renderer->getRendererType())
    {
    case RendererType::VULKAN:
       {
          VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
          std::scoped_lock lock(vulkanMutex);
          vRenderer->RecordCommandBuffers(Recording::START);
          
          camera_actor_->render(vRenderer);
          vRenderer->BindPipeline(pipelineInfo.pipeline);
          vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 0, camera_actor_->GetCameraUBODescriptorSetInfo().descriptorSet);
          vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 1, lightsManager->GetComponent<DescriptorSetInfo>()->descriptorSet);
          
          for (const auto& [name, actor] : ActorList) {
             switch (camera_actor_->CheckFrustum(actor->GetModelMatrix()) ) { 
             case FrustumCollision::CULLED_NEAR:
             case FrustumCollision::CULLED_FAR:
             case FrustumCollision::CULLED_RIGHT:
             case FrustumCollision::CULLED_LEFT:
             case FrustumCollision::CULLED_TOP:
             case FrustumCollision::CULLED_BOTTOM:
                break; 
             default:
                actor->render(vRenderer, pipelineInfo);
                break;
             }
          }
          lightsManager->render(vRenderer, pipelineInfo);
          vRenderer->RecordCommandBuffers(Recording::STOP);
          vRenderer->Render();
          break;
       }

    case RendererType::OPENGL:
       break;
    }
}

void Scene0::OnDestroy() {
    if (VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer)) {
       vkDeviceWaitIdle(vRenderer->getDevice());

       // Destroy the TLAS resources
       vRenderer->DestroyTLAS(tlas, tlasBuffer, tlasMemory, tlasInstanceBuffer, tlasInstanceMemory);

       vRenderer->DestroyPipeline(pipelineInfo);
       vRenderer->DestroyUBO(lightsManager->GetLightsUBO());
       vRenderer->DestroyUBO(camera_actor_->GetCameraUBO());
       vRenderer->DestroyDescriptorSet(camera_actor_->GetCameraUBODescriptorSetInfoRef());
       vRenderer->DestroyDescriptorSet(*lightsManager->GetComponent<DescriptorSetInfo>());
       vRenderer->DestroyPipeline(camera_actor_->GetSkyboxPipelineInfo());
        
       if (camera_actor_ && camera_actor_->GetComponent<DescriptorSetInfo>())  vRenderer->DestroyDescriptorSet(*camera_actor_->GetComponent<DescriptorSetInfo>());
       if (camera_actor_ && camera_actor_->GetComponent<SamplerCube>())  vRenderer->DestroySamplerCube(*camera_actor_->GetComponent<SamplerCube>());
       if (camera_actor_ && camera_actor_->GetComponent<IndexedVertexBuffer>()) vRenderer->DestroyIndexedMesh(*camera_actor_->GetComponent<IndexedVertexBuffer>());
       
       for (const auto& [name, actor] : ActorList) {
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