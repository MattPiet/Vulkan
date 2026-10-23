#include <Upscalers/DLSS/DLSSManager.h>
#include <iostream>
#include <core/Debug.h>

DLSSManager::DLSSManager() = default;

DLSSManager::~DLSSManager() {
    Shutdown();
}

bool DLSSManager::PreInitialize() {
    // 1. Define the features you want Streamline to load
    sl::Feature featuresToLoad[] = { sl::kFeatureDLSS };

    // 2. Define Streamline Preferences
    sl::Preferences pref = {};
    pref.applicationId = 0; 
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.showConsole = true;           
    pref.logLevel = sl::LogLevel::eDefault;
    
    // 3. Attach the features array
    pref.featuresToLoad = featuresToLoad;
    pref.numFeaturesToLoad = 1;

    // Initialize Streamline Core
    sl::Result result = slInit(pref);
    if (result != sl::Result::eOk) {
        std::cerr << "[Streamline] Failed to initialize Streamline! Error code: " << (int)result << std::endl;
        return false;
    }
    
    std::cout << "[Streamline] Core initialized successfully." << std::endl;
    return true;
}

bool DLSSManager::SetVulkanInfo(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    Device = device;
    
    // Pass Vulkan handles to Streamline so it can hook the rendering pipeline
    sl::VulkanInfo vkInfo = {};
    vkInfo.instance = instance;
    vkInfo.physicalDevice = physicalDevice;
    vkInfo.device = device;
    
    sl::Result result = slSetVulkanInfo(vkInfo);
    if (result != sl::Result::eOk) {
        std::cerr << "[Streamline] Failed to set Vulkan info! Error code: " << (int)result << std::endl;
        return false;
    }

    // Check if DLSS Super Resolution is supported on this specific Vulkan Physical Device
    sl::AdapterInfo adapterInfo = {};
    adapterInfo.vkPhysicalDevice = physicalDevice;

    result = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);

    if (result == sl::Result::eOk) {
        std::cout << "[Streamline] DLSS Super Resolution is fully supported on this GPU!" << std::endl;
        isSupported = true;
    } else {
        std::cout << "[Streamline] DLSS Super Resolution is NOT supported on this hardware. Result: " << (int)result << std::endl;
        isSupported = false;
    }

    return isSupported;
}


void DLSSManager::UpdateFrameConstants(const DLSSFrameParams& params, uint32_t frameIndex) {
    if (!isSupported) return;

    sl::ViewportHandle viewport(0);
    
    sl::FrameToken* frameToken = nullptr;
    slGetNewFrameToken(frameToken, &frameIndex); // Added & prefix for out-parameter pointer

    sl::DLSSOptions dlssOptions = {};
    dlssOptions.mode = GetDLSSMode(params.qualityMode);
    dlssOptions.outputWidth = params.displayWidth;
    dlssOptions.outputHeight = params.displayHeight;
    dlssOptions.sharpness = params.sharpness;

    slDLSSSetOptions(viewport, dlssOptions);

    sl::Constants slConsts = {};
    slConsts.cameraViewToClip = params.viewToClipMatrix;
    slConsts.clipToCameraView = params.inverseViewToClip;
    slConsts.cameraPinholeOffset = { params.jitterX, params.jitterY };

    if (frameToken) {
        slSetConstants(slConsts, *frameToken, viewport);
    }
}

void DLSSManager::TagResources(VkCommandBuffer cmdBuffer, const DLSSResourceBindings& resources) {
    if (!isSupported) return;

    auto colorRes   = sl::Resource(sl::ResourceType::eTex2d, resources.colorBufferImage);
    auto depthRes   = sl::Resource(sl::ResourceType::eTex2d, resources.depthBufferImage);
    auto mvecRes    = sl::Resource(sl::ResourceType::eTex2d, resources.motionVectorImage);
    auto outputRes  = sl::Resource(sl::ResourceType::eTex2d, resources.outputUpscaledImage);

    sl::ResourceTag colorInputTag  = { &colorRes, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eOnlyValidNow };
    sl::ResourceTag depthInputTag  = { &depthRes, sl::kBufferTypeDepth, sl::ResourceLifecycle::eOnlyValidNow };
    sl::ResourceTag mvecInputTag   = { &mvecRes, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eOnlyValidNow };
    sl::ResourceTag outputTag      = { &outputRes, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eOnlyValidNow };

    sl::ResourceTag inputs[] = { colorInputTag, depthInputTag, mvecInputTag, outputTag };
    
    sl::ViewportHandle viewport(0);
    slSetTag(viewport, inputs, 4, &cmdBuffer);
}

void DLSSManager::EvaluateDLSS(VkCommandBuffer cmdBuffer) {
    if (!isSupported) return;

    sl::FrameToken* frameToken = nullptr;
    uint32_t currentFrameIndex = 0;
    slGetNewFrameToken(frameToken, &currentFrameIndex); // Added & prefix for out-parameter pointer

    if (frameToken) {
        slEvaluateFeature(sl::kFeatureDLSS, *frameToken, nullptr, 0, cmdBuffer);
    }
}

void DLSSManager::Shutdown() {
    slShutdown();
    std::cout << "[Streamline] Shut down successfully." << std::endl;
}