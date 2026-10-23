#ifndef LIGHTS_MANAGER_H
#define LIGHTS_MANAGER_H
#include <core/Actor.h>
#include <unordered_map>
#include <string>

#include "CoreStructs.h"
#include "physics/TransformComponent.h"

class LightsManager : public Actor
{
    unsigned int maxLights = 4;
    
    std::vector<BufferMemory> LightsUBO;
    LightsData lights;
    std::unordered_map<std::string, TransformComponent*> LightTransforms;
    std::unordered_map<std::string, Matrix4> previousRenderModelMatrices_;
    
public:
    std::unordered_map<std::string, IndexedVertexBuffer*> lightMeshes;
    bool renderMeshes;
    
    LightsManager(std::weak_ptr<Component> parent) : Actor(parent) {}
    bool OnCreate(VulkanRenderer* vRenderer) override;
    void OnDestroy() override;
    
    std::vector<BufferMemory> GetLightsUBO() const { return LightsUBO; }
    LightsData GetLightsData() const { return lights; }
    void setPosition(const std::string& lightNumber, const Vec3& pos)
    {
        LightTransforms.at(lightNumber)->SetPosition(pos);
        lights.pos[stoi(lightNumber)] = pos;
    }
    std::unordered_map<std::string, TransformComponent*> getLightTransforms() const { return LightTransforms; }
    void render(VulkanRenderer* vRenderer, PipelineInfo pipelineInfo);
    void ResetMotionHistory() { previousRenderModelMatrices_.clear(); }
    
    /*DescriptorSetInfo GetLightsUBODescriptorSetInfo() const { return LightsUBOinfo; }
    DescriptorSetInfo& GetLightsUBODescriptorSetInfoRef() { return LightsUBOinfo; }
    void SetLightsUBODescriptorSetInfo(const DescriptorSetInfo& info) { LightsUBOinfo = info; }*/
};
#endif