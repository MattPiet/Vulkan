#pragma once
#include <vulkan/vulkan.h>
#include <Vector.h>
#include <VMath.h>
#include <MMath.h>
#include <Hash.h>
#include <core/Component.h>
using namespace MATH;

struct BufferMemory {
    VkBuffer bufferID;
    VkDeviceMemory bufferMemoryID;
    VkDeviceSize bufferMemoryLength;
};

struct Sampler2D : Component {
    Sampler2D(std::weak_ptr<Component> parent_ = std::weak_ptr<Component>()) : Component(parent_) {}
    VkImage image;
    VkDeviceMemory imageDeviceMemory;
    VkImageView imageView;
    VkSampler sampler;
    std::string filename;
    virtual bool OnCreate() override{return true;}
    virtual void OnDestroy() override{}
    virtual void Update(const float deltaTime_) override{}
    virtual void Render() const override{}
};

struct IndexedVertexBuffer : Component {
    IndexedVertexBuffer(std::weak_ptr<Component> parent_ = std::weak_ptr<Component>()) : Component(parent_) {}
    VkBuffer vertBufferID;
    VkDeviceMemory vertBufferMemoryID;
    VkDeviceSize vertBufferLength;

    VkBuffer indexBufferID;
    VkDeviceMemory indexBufferMemoryID;
    VkDeviceSize indexBufferLength;
    
    std::string filename;
    
    virtual bool OnCreate() override{return true;}
    virtual void OnDestroy() override{}
    virtual void Update(const float deltaTime_) override{}
    virtual void Render() const override{}
};

struct CameraData { 
    Matrix4 projectionMatrix;
    Matrix4 viewMatrix;
};

#define MAX_LIGHTS 4 
struct LightsData {
    Vec4 pos[MAX_LIGHTS];
    Vec4 diffuse[MAX_LIGHTS];
    Vec4 specular[MAX_LIGHTS];
    Vec4 ambient;
    uint32_t numLights = 0;
};



/// A 3x3 cannot be sent to the GPU data alignment issues. 
/// If I try to send a 3x3 yo GPU it will be bumped up to a 4x4. 
/// I can fake it by storing the 3x3 in an array of 3 Vec4s as 
/// I have mapped below. Vulkan and OpenGl are column centric - right hand rule
/// The real reason to do this is to make room in push constant for other data.
///	Vec4    0(x)	3(y)	6(z)    0(w)			
///	Vec4    1(x)	4(y)	7(z)    0(w)		
///	Vec4    2(x)	5(y)	9(z)    0(w)		
struct ModelMatrixPushConst {
    Matrix4 modelMatrix;
    Vec4 normalMatrix[3];
    Vec4 pad;/// padding for right now
};


struct DescriptorSetInfo : Component {
    DescriptorSetInfo(std::weak_ptr<Component> parent_ = std::weak_ptr<Component>()) : Component(parent_) {}
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSet;
    
    std::string VertFilename;
    std::string FragFilename;
        
    virtual bool OnCreate() override{return true;}
    virtual void OnDestroy() override{}
    virtual void Update(const float deltaTime_) override{}
    virtual void Render() const override{}
};

struct PipelineInfo {
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};

struct RenderData {
    VkPipeline pipeline;
    IndexedVertexBuffer mesh;
    DescriptorSetInfo descriptorSetInfo;
    ModelMatrixPushConst modelMatrixPushConst;
};

struct CommandBufferData {
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
};


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

