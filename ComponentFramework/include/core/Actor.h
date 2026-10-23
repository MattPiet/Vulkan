#pragma once
#include <iostream>
#include <string>
#include <typeinfo>
#include <Matrix.h>
#include <Core/Component.h>
#include <memory>
#include <core/ML.h>

#include "CoreStructs.h"

class Actor : public Component {
    Actor(const Actor&) = delete;
    Actor(Actor&&) = delete;
    Actor& operator= (const Actor&) = delete;
    Actor& operator=(Actor&&) = delete;

protected:
    // Trashed the vector—everything now lives in our custom ML::Map!
    ML::Map<std::string, std::shared_ptr<Component>> componentMap;

public:
    Actor(std::weak_ptr<Component> parent_);
    virtual ~Actor();
    virtual bool OnCreate(VulkanRenderer* vRenderer) override;
    virtual void OnDestroy() override;
    virtual void Update(const float deltaTime) override;
    virtual void Render() const override;
   void render(VulkanRenderer* vRenderer, PipelineInfo pipelineInfo,
               const MATH::Matrix4* previousModelMatrix = nullptr);

    template<typename ComponentTemplate>
    void AddComponent(std::shared_ptr<ComponentTemplate> component_) {
       std::string typeKey = typeid(ComponentTemplate).name();
       
       if (componentMap.find(typeKey) != nullptr) {
       #ifdef _DEBUG
          std::cerr << "WARNING: Trying to add a component type that is already added - ignored\n";
       #endif
          return;
       }
       
       std::shared_ptr<Component> baseComponent = component_;
       std::shared_ptr<Component> dummy;
       componentMap.findOrInsert(typeKey, baseComponent, dummy);
    }

    template<typename ComponentTemplate, typename ... Args>
    void AddComponent(Args&& ... args_) {
       std::string typeKey = typeid(ComponentTemplate).name();
       
       if (componentMap.find(typeKey) != nullptr) {
       #ifdef _DEBUG
          std::cerr << "WARNING: Trying to add a component type that is already added - ignored\n";
       #endif
          return;
       }
       
       std::shared_ptr<Component> newComponent = std::make_shared<ComponentTemplate>(std::forward<Args>(args_)...);
       std::shared_ptr<Component> dummy;
       componentMap.findOrInsert(typeKey, newComponent, dummy);
    }

   template<typename ComponentTemplate>
   Ref<ComponentTemplate> GetComponent() const {
       std::string typeKey = typeid(ComponentTemplate).name();
       
       // Use a const pointer since componentMap is accessed from a const method
       const std::shared_ptr<Component>* foundComponentPtr = componentMap.find(typeKey);
       if (foundComponentPtr && *foundComponentPtr) {
          return std::dynamic_pointer_cast<ComponentTemplate>(*foundComponentPtr);
       }
       
       return Ref<ComponentTemplate>(nullptr);
    }

    template<typename ComponentTemplate>
    void RemoveComponent() {
       // Optional: if you want fine-grained removal by type later
       // (Handled via clear/destroy currently)
    }

    void RemoveAllComponents() {
       componentMap.clear();
    }

    void ListComponents() const;

    MATH::Matrix4 GetModelMatrix();
};
