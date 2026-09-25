/*#include <Upscalers/DLSS/DLSSManager.h>
#include <iostream>

DLSSManager::DLSSManager() = default;

DLSSManager::~DLSSManager() {
    Shutdown();
}

bool DLSSManager::Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t appId, const wchar_t* dataPath) {
    m_device = device;

    // Define Streamline Preferences
    sl::Preferences pref = {};
    pref.applicationId = appId;
    pref.pathToLogsAndData = dataPath;
    pref.pathsToPlugins = &dataPath;
    pref.numPathsToPlugins = 1;
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.showConsole = true;           
    pref.logLevel = sl::LogLevel::eDefault;

    // Initialize Streamline Core
    sl::Result result = slInit(pref, SL_VERSION_PATCH);
    if (result != sl::Result::eOk) {
        std::cerr << "[Streamline] Failed to initialize Streamline! Error code: " << (int)result << std::endl;
        return false;
    }

    // Set Vulkan Device Info for Streamline
    sl::VulkanInfo vkInfo = {};
    vkInfo.instance = instance;
    vkInfo.physicalDevice = physicalDevice;
    vkInfo.device = device;
    
    result = slSetVulkanInfo(vkInfo);
    if (result != sl::Result::eOk) {
        std::cerr << "[Streamline] Failed to set Vulkan info! Error code: " << (int)result << std::endl;
        return false;
    }

    // Check if DLSS Super Resolution is supported on this specific Vulkan Physical Device
    sl::AdapterInfo adapterInfo = {};
    adapterInfo.vkPhysicalDevice = physicalDevice;

    bool supported = false;
    result = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);

    if (result == sl::Result::eOk && supported) {
        std::cout << "[Streamline] DLSS Super Resolution is fully supported on this GPU!" << std::endl;
        m_isSupported = true;
    } else {
        std::cout << "[Streamline] DLSS Super Resolution is NOT supported on this hardware. Result: " << (int)result << std::endl;
        m_isSupported = false;
    }

    return m_isSupported;
}

void DLSSManager::UpdateFrameConstants(const DLSSFrameParams& params, uint32_t frameIndex) {
    if (!m_isSupported) return;

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
    if (!m_isSupported) return;

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
    if (!m_isSupported) return;

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
}*/