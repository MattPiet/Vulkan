#ifndef LIGHTS_MANAGER_H
#define LIGHTS_MANAGER_H
#include <core/Actor.h>

#include "CoreStructs.h"

class LightsManager : public Actor
{
    unsigned int maxLights = 4;
    
    std::vector<BufferMemory> LightsUBO;
    LightsData lights;
    
public:
    
    LightsManager(std::weak_ptr<Component> parent) : Actor(parent) {}
    bool OnCreate(VulkanRenderer* vRenderer) override;
    void OnDestroy() override;
    
    std::vector<BufferMemory> GetLightsUBO() const { return LightsUBO; }
    LightsData GetLightsData() const { return lights; }
    /*DescriptorSetInfo GetLightsUBODescriptorSetInfo() const { return LightsUBOinfo; }
    DescriptorSetInfo& GetLightsUBODescriptorSetInfoRef() { return LightsUBOinfo; }
    void SetLightsUBODescriptorSetInfo(const DescriptorSetInfo& info) { LightsUBOinfo = info; }*/
};
#endif