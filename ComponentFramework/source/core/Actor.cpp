#include <Core/Actor.h>
#include <Core/Debug.h>
#include <physics/TransformComponent.h>

#include "core/CoreStructs.h"
#include "render/VulkanRenderer.h"
Actor::Actor(std::weak_ptr<Component> parent_):Component(parent_) {}

bool Actor::OnCreate(VulkanRenderer* vRenderer) {
    if (vRenderer)
    {
        if (isCreated) return true;
        Debug::Info("Loading assets for Actor: ", __FILE__, __LINE__);
    
        if (
                  GetComponent<DescriptorSetInfo>()   == nullptr ||
                  GetComponent<Sampler2D>()			  == nullptr ||
                  GetComponent<IndexedVertexBuffer>() == nullptr)
        {
            Debug::Error("Actor is missing a shader, material, mesh or shape: " + GetComponent<Sampler2D>()->filename, __FILE__, __LINE__);
           
        }
        else
        {
            *GetComponent<Sampler2D>() =  vRenderer->Create2DTextureImage(GetComponent<Sampler2D>()->filename.c_str());
            *GetComponent<IndexedVertexBuffer>() = vRenderer->LoadModelIndexed(GetComponent<IndexedVertexBuffer>()->filename.c_str());
			          
            std::string vertName = GetComponent<DescriptorSetInfo>()->VertFilename;
            std::string fragName = GetComponent<DescriptorSetInfo>()->FragFilename;
            {
                DescriptorSetBuilder actorDescriptorBuilder(vRenderer->getDevice());
                actorDescriptorBuilder.add(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, GetComponent<Sampler2D>().get());
                *GetComponent<DescriptorSetInfo>() = actorDescriptorBuilder.BuildDescriptorSet(vRenderer->getNumSwapchains());
            }
    
        isCreated = true;
        }
    }
    return true;
}

Actor::~Actor() {
    OnDestroy();
}

void Actor::OnDestroy() {
    Debug::Info("Deleting assets for Actor: ", __FILE__, __LINE__);
    RemoveAllComponents();
    isCreated = false;
}

void Actor::Update(const float deltaTime) {
    std::cout << "Hello from Update\n";
}

void Actor::Render() const {}


void Actor::ListComponents() const {
    std::cout << typeid(*this).name() << " contains the following components:\n";

    std::cout << '\n';
}

MATH::Matrix4 Actor::GetModelMatrix() {
    MATH::Matrix4 modelMatrix;
    Ref<TransformComponent> transform = GetComponent<TransformComponent>();

    if (transform) {
       modelMatrix = transform->GetTransformMatrix();
    }
    else {
       modelMatrix.loadIdentity();
    }

    if (auto lockedParent = parent.lock()) {
       // Fixed the dynamic cast for the raw pointer from the weak_ptr
       Actor* parentActor = dynamic_cast<Actor*>(lockedParent.get());
       if (parentActor) {
          auto parentTransform = parentActor->GetComponent<TransformComponent>();
          if (parentTransform) {
              // Fixed the matrix multiplication operator syntax
              modelMatrix = parentTransform->GetTranslate_Rotate_Matrix() * modelMatrix;
          }
       }
    }
    return modelMatrix;
}