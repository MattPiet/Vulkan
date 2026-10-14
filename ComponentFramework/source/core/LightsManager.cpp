#include <core/LightsManager.h>

#include "render/DescriptorSetBuilder.h"
#include "render/VulkanRenderer.h"
bool LightsManager::OnCreate(VulkanRenderer* vRenderer)
{
    if (vRenderer)
    {
    if (isCreated) return true;
    isCreated = true;
    // 0
    lights.diffuse[0] = Vec4(0.0, 0.0, 0.9, 0.0);
    lights.specular[0] = Vec4(0.0, 0.0, 0.3, 0.0);
    lights.ambient = Vec4(0.01, 0.01, 0.01, 0.0);
    lights.numLights = 4;
    lights.pos[0] = Vec4(-4.0f, -5.0f, -5.0f, 0.0f);
    // 1
    lights.diffuse[1] = Vec4(0.0, 0.9, 0.0, 0.0);
    lights.specular[1] = Vec4(0.0, 0.3, 0.0, 0.0);
    lights.pos[1] = Vec4(-4.0f, 5.0f, -5.0f, 0.0f);
    // 2
    lights.diffuse[2] = Vec4(0.9, 0.0, 0.0, 0.0);
    lights.specular[2] = Vec4(0.3, 0.0, 0.0, 0.0);
    lights.pos[2] = Vec4(4.0f, 5.0f, -5.0f, 0.0f);
    // 2
    lights.diffuse[3] = Vec4(0.5, 0.0, 0.5, 0.0);
    lights.specular[3] = Vec4(0.1, 0.0, 0.1, 0.0);
    lights.pos[3] = Vec4(4.0f, -5.0f, -5.0f, 0.0f);
    
   
        // Very simple just makes the UBO and descriptor set for the lights data.
        LightsUBO = vRenderer->CreateUniformBuffers<LightsData>();
        DescriptorSetBuilder LightsDescriptorBuilder(vRenderer->getDevice());
        LightsDescriptorBuilder.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, LightsUBO);
        *GetComponent<DescriptorSetInfo>() = LightsDescriptorBuilder.BuildDescriptorSet(vRenderer->getNumSwapchains());
        vRenderer->UpdateUniformBuffer<LightsData>(lights, LightsUBO);
    }    
    return true;
}

void LightsManager::OnDestroy()
{
    Actor::OnDestroy();
}
