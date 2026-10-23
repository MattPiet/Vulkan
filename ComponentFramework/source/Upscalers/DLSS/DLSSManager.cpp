#include <Upscalers/DLSS/DLSSManager.h>

#include <iostream>

namespace {

void LogSLFailure(const char* operation, sl::Result result)
{
    std::cerr << "[DLSS] " << operation << " failed with Streamline result "
              << static_cast<int>(result) << '\n';
}

bool IsValidImage(const DLSSImageBinding& image)
{
    return image.image != VK_NULL_HANDLE &&
           image.view != VK_NULL_HANDLE &&
           image.layout != VK_IMAGE_LAYOUT_UNDEFINED &&
           image.format != VK_FORMAT_UNDEFINED &&
           image.width > 0 && image.height > 0 &&
           image.mipLevels > 0 && image.arrayLayers > 0;
}

sl::Resource MakeSLResource(const DLSSImageBinding& image)
{
    // VkImage/VkImageView/VkDeviceMemory are pointer-shaped handles on the
    // project's 64-bit Windows Vulkan build. Streamline's Vulkan Resource stores
    // these handles as void pointers.
    sl::Resource resource(
        sl::ResourceType::eTex2d,
        reinterpret_cast<void*>(image.image),
        reinterpret_cast<void*>(image.memory),
        reinterpret_cast<void*>(image.view),
        static_cast<uint32_t>(image.layout));

    // Vulkan cannot query an image's description from VkImage, so supply it.
    resource.width = image.width;
    resource.height = image.height;
    resource.nativeFormat = static_cast<uint32_t>(image.format);
    resource.mipLevels = image.mipLevels;
    resource.arrayLayers = image.arrayLayers;
    resource.flags = image.flags;
    resource.usage = image.usage;
    return resource;
}

sl::Boolean ToSLBoolean(bool value)
{
    return value ? sl::Boolean::eTrue : sl::Boolean::eFalse;
}

} // namespace

DLSSManager::DLSSManager() = default;

DLSSManager::~DLSSManager() = default;

sl::DLSSMode DLSSManager::GetDLSSMode(DLSSQualityMode mode) const
{
    switch (mode) {
    case DLSSQualityMode::Performance:      return sl::DLSSMode::eMaxPerformance;
    case DLSSQualityMode::Balanced:         return sl::DLSSMode::eBalanced;
    case DLSSQualityMode::Quality:          return sl::DLSSMode::eMaxQuality;
    case DLSSQualityMode::UltraPerformance: return sl::DLSSMode::eUltraPerformance;
    case DLSSQualityMode::DLAA:             return sl::DLSSMode::eDLAA;
    default:                                return sl::DLSSMode::eOff;
    }
}

bool DLSSManager::InitializeStreamline()
{
    if (streamlineInitialized) {
        return true;
    }

    sl::Preferences pref{};
    pref.showConsole = true;
    pref.logLevel = sl::LogLevel::eDefault;
    pref.engine = sl::EngineType::eCustom;
    pref.engineVersion = "1.0.0";
    pref.projectId = "b8cc3437-a81c-4a14-bffd-a4bda51c4d49";
    pref.pathsToPlugins = nullptr;
    pref.numPathsToPlugins = 0;
    pref.renderAPI = sl::RenderAPI::eVulkan;

    // Required when using slSetTagForFrame for frame-specific tags.
    pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;

    sl::Feature featuresToLoad[] = { sl::kFeatureDLSS };
    pref.featuresToLoad = featuresToLoad;
    pref.numFeaturesToLoad = 1;

    sl::Result result = slInit(pref);
    if (result != sl::Result::eOk) {
        LogSLFailure("slInit", result);
        return false;
    }

    streamlineInitialized = true;
    std::cout << "[Streamline] Core initialized successfully.\n";

    result = slGetFeatureRequirements(sl::kFeatureDLSS, dlssRequirements);
    if (result != sl::Result::eOk) {
        LogSLFailure("slGetFeatureRequirements", result);
        Shutdown();
        return false;
    }

    if ((dlssRequirements.flags & sl::FeatureRequirementFlags::eVulkanSupported) == 0) {
        std::cerr << "[DLSS] Streamline reports that DLSS does not support Vulkan.\n";
        Shutdown();
        return false;
    }

    std::cout << "[DLSS] Instance extensions: " << dlssRequirements.vkNumInstanceExtensions << '\n';
    std::cout << "[DLSS] Device extensions: " << dlssRequirements.vkNumDeviceExtensions << '\n';
    std::cout << "[DLSS] Vulkan 1.2 features: " << dlssRequirements.vkNumFeatures12 << '\n';
    std::cout << "[DLSS] Vulkan 1.3 features: " << dlssRequirements.vkNumFeatures13 << '\n';
    std::cout << "[Streamline] DLSS requirements retrieved successfully.\n";
    return true;
}

bool DLSSManager::AttachVulkan(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device)
{
    (void)instance; // Not needed by this attachment path.

    if (!streamlineInitialized)
    {
        std::cerr << "[DLSS] Streamline is not initialized.\n";
        return false;
    }

    Device = device;

    // Do not call slSetVulkanInfo() in the current integration path.
    // It previously returned eErrorInvalidIntegration.
    sl::AdapterInfo adapterInfo{};
    adapterInfo.vkPhysicalDevice = physicalDevice;

    const sl::Result result =
        slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);

    std::cout << "[DLSS] Post-device support result: "
              << static_cast<int>(result) << '\n';

    isSupported = (result == sl::Result::eOk);
    return isSupported;
}

bool DLSSManager::CheckAdapterSupport(VkPhysicalDevice physicalDevice)
{
    if (!streamlineInitialized) {
        return false;
    }

    sl::AdapterInfo adapterInfo{};
    adapterInfo.vkPhysicalDevice = physicalDevice;

    const sl::Result result = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);
    std::cout << "[DLSS] Pre-device support result: " << static_cast<int>(result) << '\n';
    if (result != sl::Result::eOk) {
        LogSLFailure("slIsFeatureSupported (pre-device)", result);
        return false;
    }

    return true;
}

bool DLSSManager::GetOptimalRenderResolution(
    uint32_t outputWidth,
    uint32_t outputHeight,
    DLSSQualityMode qualityMode,
    uint32_t& renderWidth,
    uint32_t& renderHeight)
{
    if (!isSupported || outputWidth == 0 || outputHeight == 0) {
        return false;
    }

    sl::DLSSOptions options{};
    options.mode = GetDLSSMode(qualityMode);
    options.outputWidth = outputWidth;
    options.outputHeight = outputHeight;
    options.colorBuffersHDR = sl::Boolean::eFalse;
    options.useAutoExposure = sl::Boolean::eTrue;

    sl::DLSSOptimalSettings settings{};
    const sl::Result result = slDLSSGetOptimalSettings(options, settings);
    if (result != sl::Result::eOk || settings.optimalRenderWidth == 0 || settings.optimalRenderHeight == 0) {
        if (result != sl::Result::eOk) LogSLFailure("slDLSSGetOptimalSettings", result);
        return false;
    }

    renderWidth = settings.optimalRenderWidth;
    renderHeight = settings.optimalRenderHeight;
    return true;
}

bool DLSSManager::UpdateFrameConstants(const DLSSFrameParams& params, uint32_t frameIndex)
{
    currentFrameToken = nullptr;
    if (!isSupported) {
        return false;
    }

    if (params.renderWidth == 0 || params.renderHeight == 0 ||
        params.displayWidth == 0 || params.displayHeight == 0) {
        std::cerr << "[DLSS] Invalid render/display dimensions in frame parameters.\n";
        return false;
    }

    sl::Result result = slGetNewFrameToken(currentFrameToken, &frameIndex);
    if (result != sl::Result::eOk || currentFrameToken == nullptr) {
        if (result != sl::Result::eOk) LogSLFailure("slGetNewFrameToken", result);
        else std::cerr << "[DLSS] slGetNewFrameToken returned a null frame token.\n";
        currentFrameToken = nullptr;
        return false;
    }

    const sl::ViewportHandle viewport(0);

    sl::DLSSOptions dlssOptions{};
    dlssOptions.mode = GetDLSSMode(params.qualityMode);
    dlssOptions.outputWidth = params.displayWidth;
    dlssOptions.outputHeight = params.displayHeight;
    dlssOptions.colorBuffersHDR = ToSLBoolean(params.colorBuffersHDR);
    // No exposure image is currently supplied, so explicitly request auto exposure.
    dlssOptions.useAutoExposure = sl::Boolean::eTrue;

    result = slDLSSSetOptions(viewport, dlssOptions);
    if (result != sl::Result::eOk) {
        LogSLFailure("slDLSSSetOptions", result);
        currentFrameToken = nullptr;
        return false;
    }

    sl::Constants constants{};
    constants.cameraViewToClip = params.viewToClipMatrix;
    constants.clipToCameraView = params.inverseViewToClip;
    constants.clipToPrevClip = params.clipToPrevClipMatrix;
    constants.prevClipToClip = params.prevClipToClipMatrix;
    constants.jitterOffset = { params.jitterX, params.jitterY };
    constants.mvecScale = params.motionVectorScale;
    constants.cameraPos = params.cameraPosition;
    constants.cameraUp = params.cameraUp;
    constants.cameraRight = params.cameraRight;
    constants.cameraFwd = params.cameraForward;
    constants.cameraNear = params.cameraNear;
    constants.cameraFar = params.cameraFar;
    constants.cameraFOV = params.cameraFOV;
    constants.cameraAspectRatio = params.cameraAspectRatio;
    constants.motionVectorsInvalidValue = params.motionVectorsInvalidValue;
    constants.depthInverted = ToSLBoolean(params.depthInverted);
    constants.cameraMotionIncluded = ToSLBoolean(params.cameraMotionIncluded);
    constants.motionVectors3D = ToSLBoolean(params.motionVectors3D);
    constants.reset = ToSLBoolean(params.isCameraCut);
    constants.orthographicProjection = ToSLBoolean(params.orthographicProjection);
    constants.motionVectorsDilated = ToSLBoolean(params.motionVectorsDilated);
    constants.motionVectorsJittered = ToSLBoolean(params.motionVectorsJittered);

    result = slSetConstants(constants, *currentFrameToken, viewport);
    if (result != sl::Result::eOk) {
        LogSLFailure("slSetConstants", result);
        currentFrameToken = nullptr;
        return false;
    }

    return true;
}

bool DLSSManager::TagResources(
    VkCommandBuffer cmdBuffer,
    const DLSSResourceBindings& resources)
{
    if (!isSupported || currentFrameToken == nullptr || cmdBuffer == VK_NULL_HANDLE) {
        std::cerr << "[DLSS] TagResources called without a supported feature, frame token, or command buffer.\n";
        return false;
    }

    if (!IsValidImage(resources.colorBuffer) ||
        !IsValidImage(resources.depthBuffer) ||
        !IsValidImage(resources.motionVectorBuffer) ||
        !IsValidImage(resources.outputUpscaledBuffer)) {
        std::cerr << "[DLSS] Invalid Vulkan image binding. Each image needs a valid image/view, layout, format, dimensions, and subresource counts.\n";
        return false;
    }

    sl::Resource color = MakeSLResource(resources.colorBuffer);
    sl::Resource depth = MakeSLResource(resources.depthBuffer);
    sl::Resource motionVectors = MakeSLResource(resources.motionVectorBuffer);
    sl::Resource output = MakeSLResource(resources.outputUpscaledBuffer);

    const sl::Extent colorExtent{ 0, 0, resources.colorBuffer.width, resources.colorBuffer.height };
    const sl::Extent depthExtent{ 0, 0, resources.depthBuffer.width, resources.depthBuffer.height };
    const sl::Extent motionVectorExtent{ 0, 0, resources.motionVectorBuffer.width, resources.motionVectorBuffer.height };
    const sl::Extent outputExtent{ 0, 0, resources.outputUpscaledBuffer.width, resources.outputUpscaledBuffer.height };

    sl::ResourceTag tags[] = {
        { &color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &colorExtent },
        { &depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &depthExtent },
        { &motionVectors, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &motionVectorExtent },
        { &output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eOnlyValidNow, &outputExtent }
    };

    const sl::ViewportHandle viewport(0);
    // VkCommandBuffer is itself a handle/pointer. Pass the handle, not &cmdBuffer.
    const sl::Result result = slSetTagForFrame(*currentFrameToken, viewport, tags, 4, cmdBuffer);
    if (result != sl::Result::eOk) {
        LogSLFailure("slSetTagForFrame", result);
        return false;
    }

    return true;
}

bool DLSSManager::EvaluateDLSS(VkCommandBuffer cmdBuffer)
{
    if (!isSupported || currentFrameToken == nullptr || cmdBuffer == VK_NULL_HANDLE) {
        std::cerr << "[DLSS] EvaluateDLSS called without a supported feature, frame token, or command buffer.\n";
        currentFrameToken = nullptr;
        return false;
    }

    const sl::ViewportHandle viewport(0);
    const sl::BaseStructure* inputs[] = { &viewport };
    const sl::Result result = slEvaluateFeature(
        sl::kFeatureDLSS,
        *currentFrameToken,
        inputs,
        1,
        cmdBuffer);

    currentFrameToken = nullptr;

    if (result != sl::Result::eOk) {
        LogSLFailure("slEvaluateFeature(DLSS)", result);
        return false;
    }

    if (!hasLoggedSuccessfulEvaluation) {
        std::cout << "[DLSS] First slEvaluateFeature call succeeded.\n";
        hasLoggedSuccessfulEvaluation = true;
    }
    return true;
}

void DLSSManager::Shutdown()
{
    currentFrameToken = nullptr;
    isSupported = false;
    hasLoggedSuccessfulEvaluation = false;
    Device = VK_NULL_HANDLE;

    if (!streamlineInitialized) {
        return;
    }

    const sl::Result result = slShutdown();
    if (result != sl::Result::eOk) {
        LogSLFailure("slShutdown", result);
    } else {
        std::cout << "[Streamline] Shut down successfully.\n";
    }

    streamlineInitialized = false;
}
