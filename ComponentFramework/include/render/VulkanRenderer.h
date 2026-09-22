#ifndef VULKANRENDERER_H 
#define VULKANRENDERER_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <array>
#include <queue>

#include "core/CoreStructs.h"
#include "Renderer.h"
#include "DescriptorSetBuilder.h"


#include <VMath.h>
#include <MMath.h>
#include <Hash.h>
using namespace MATH;


#ifdef NDEBUG /// only use validation layers if in debug mode
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


struct QueueFamilyIndices {
    /// optional means that it contains no value until it is assigned.
    /// has_value() returns false if no value has ever been assigned. 
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};



struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 texCoord;

    /// Used in vertex deduplication. The operator == will compare two verticies and determine if
    /// they are exactly identical.  
    bool operator == (const Vertex& other) const {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
    }   

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {

        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
       
}; 


/// Generate a hash of a Vertex, used in vertex deduplication
/// Adding this to namespace std is called a namespace injection
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const noexcept {
            size_t hash1 = hash<Vec3>()(vertex.pos);
            size_t hash2 = hash<Vec3>()(vertex.normal);
            size_t hash3 = hash<Vec2>()(vertex.texCoord);
            size_t result = ((hash1 ^ (hash2 << 1)) >> 1) ^ (hash3 << 1);
            return result;
        }
    };
}



enum Recording {
    START,
    STOP
};

class VulkanRenderer : public Renderer {
    /// C11 precautions 
    VulkanRenderer(const VulkanRenderer&) = delete;  /// Copy constructor
    VulkanRenderer(VulkanRenderer&&) = delete;       /// Move constructor
    VulkanRenderer& operator=(const VulkanRenderer&) = delete; /// Copy operator
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;      /// Move operator


public: /// Member functions
    VulkanRenderer();
    ~VulkanRenderer();

    
    SDL_Window* CreateWindow(std::string name_, int width, int height);
    bool OnCreate();
    void OnDestroy();
    void Render();
    void RecreateSwapChain();

  


    /// See VulkanSampler.cpp
    Sampler2D Create2DTextureImage(const char* textureFile);
    void createTextureImageView(Sampler2D& texture2D);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void createTextureSampler(Sampler2D& texture2D) const;
    void DestroySampler2D(Sampler2D& texture2D);
    

    /// See DescriptorSetBuilder.cpp
    void BindDescriptorSet(VkPipelineLayout pipelineLayout, int firstSet ,const std::vector<VkDescriptorSet> descriptorSet);
    void BindMultipleDescriptorSets(VkPipelineLayout pipelineLayout, std::vector<std::vector<VkDescriptorSet>> descriptorSets);
    void DestroyDescriptorSet(DescriptorSetInfo& descriptorSetInfo);
    

    /// See VulkanPipeline.cpp
    PipelineInfo CreateGraphicsPipeline(VkDescriptorSetLayout descriptorSetLayout, const char* vertFile, const char* fragFile,
        const char* tessCtrlFile = nullptr, const char* tessEvalFile = nullptr, const char* geomFile = nullptr);
    PipelineInfo CreateGraphicsPipeline(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const char* vertFile, const char* fragFile, 
        const char* tessCtrlFile = nullptr, const char* tessEvalFile = nullptr, const char* geomFile = nullptr);
    
    void BindPipeline(VkPipeline pipeline);
    void DestroyPipeline(PipelineInfo pipeline);
    static std::vector<char> readFile(const std::string& filename);


    /// See VulkanMesh.cpp
    IndexedVertexBuffer LoadModelIndexed(const char* filename);
    void BindMesh(IndexedVertexBuffer mesh);
    void DrawIndexed(IndexedVertexBuffer mesh);
    void DestroyIndexedMesh(IndexedVertexBuffer mesh_);
private:
    void CreateVertexBuffer(IndexedVertexBuffer& indexedVertexBuffer, const std::vector<Vertex>& vertices);
    void CreateIndexBuffer(IndexedVertexBuffer& indexedVertexBuffer, const std::vector<uint32_t>& indices);
        /// A helper function for createVertexBuffer()
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    

public:
    /// See VulkanCommandBuffer.cpp
    void createCommandPool();
    void CreateCommandBuffers();
    void DestroyCommandBuffers();
    void RecordCommandBuffers(Recording start_stop);
    void SetPushConstant(const PipelineInfo pipelineInfo, const Matrix4& modelMatrix_);
    
    /// Some inlined getter and setters 
    SDL_Window* getWindow() { return window; }
    VkDevice getDevice() { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    uint32_t getNumSwapchains() { return numSwapchains; }
   

    template<class T>
    std::vector<BufferMemory> CreateUniformBuffers() {
        std::vector<BufferMemory> ubo;
        VkDeviceSize bufferSize = sizeof(T);
        ubo.resize(numSwapchains);
        for (size_t i = 0; i < numSwapchains; i++) {
            ubo[i].bufferMemoryLength = bufferSize;
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                ubo[i].bufferID, ubo[i].bufferMemoryID);
        }
        return ubo;
    }

    template <class T>
    void UpdateUniformBuffer(const T& srcData, const std::vector<BufferMemory> bufferMemory) {
        void* data;
        for (int i = 0; i < numSwapchains; ++i) {
            vkMapMemory(device, bufferMemory[i].bufferMemoryID, 0, sizeof(T), 0, &data);
            memcpy(data, &srcData, bufferMemory[i].bufferMemoryLength);
            vkUnmapMemory(device, bufferMemory[i].bufferMemoryID);
        }
    };

    void DestroyUBO(std::vector<BufferMemory> ubo);


private: /// Private member variables
    const uint32_t MAX_FRAMES_IN_FLIGHT;
    uint32_t numSwapchains;
    uint32_t currentFrame;
    SDL_Event sdlEvent;
    uint32_t windowWidth;
    uint32_t windowHeight;
    SDL_Window* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkRenderPass renderPass;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

   
    VkQueue graphicsQueue;
    VkQueue presentQueue;
 
    CommandBufferData primaryCommandBuffer{};
    std::vector<Sampler2D> textures2D;
    std::vector<BufferMemory> cameraUboBuffers;
    std::vector<BufferMemory> lightsUboBuffers;
    std::queue<ModelMatrixPushConst> pushConstant;
    std::vector<IndexedVertexBuffer> indexedVertexBuffers;

private:
    bool hasStencilComponent(VkFormat format);
    void CreateInstance();
    void createSurface();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    
   

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void createRenderPass();
    void createFramebuffers();
    void createDepthResources();
   
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    


  


    void createSyncObjects();
    void destroySwapChain();
   
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger();

    

    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    std::vector<const char*> getRequiredExtensions();
    bool checkValidationLayerSupport();

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
            VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);


    

    /// Used to reformat the Validation Layer's output to the console
    static void insertLineBreaks(std::string& str, size_t maxWidth) {
        size_t index = 0;
        while (index + maxWidth < str.size()) {
            /// Find a blank space before the maxWidth is encountered
            size_t breakLocation  = str.find(' ', index + maxWidth);
            if (breakLocation == std::string::npos || breakLocation <= index) {
                /// If no blank space, just break at maxWidth
                breakLocation = index + maxWidth;
            }
            str[breakLocation] = '\n';
            index = breakLocation + 1;
        }
    }

};
#endif 

