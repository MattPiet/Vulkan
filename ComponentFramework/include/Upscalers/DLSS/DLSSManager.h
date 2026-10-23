#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <sl.h>
#include <sl_dlss.h>
#include <sl_helpers_vk.h>

enum class DLSSQualityMode;

// Per-frame values supplied by the renderer/camera.
// Matrix and motion-vector conventions must match the values used to render the frame.
struct DLSSFrameParams {
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;

    // Pixel-space jitter. Do not put these values into cameraPinholeOffset.
    float jitterX = 0.0f;
    float jitterY = 0.0f;

    // Retained for compatibility with the caller; current Streamline headers mark
    // DLSSOptions::sharpness deprecated/unsupported.
    float sharpness = 0.0f;
    bool isCameraCut = false;
    DLSSQualityMode qualityMode;

    sl::float4x4 viewToClipMatrix{};
    sl::float4x4 inverseViewToClip{};
    sl::float4x4 clipToPrevClipMatrix{};
    sl::float4x4 prevClipToClipMatrix{};

    // Scale motion vectors so their values are normalized to [-1, 1].
    // Use {1,1} if the shader already writes normalized vectors, or
    // {1/renderWidth, 1/renderHeight} when it writes pixel-space vectors.
    sl::float2 motionVectorScale{ 1.0f, 1.0f };

    sl::float3 cameraPosition{};
    sl::float3 cameraUp{};
    sl::float3 cameraRight{};
    sl::float3 cameraForward{};

    float cameraNear = sl::INVALID_FLOAT;
    float cameraFar = sl::INVALID_FLOAT;
    float cameraFOV = sl::INVALID_FLOAT; // radians
    float cameraAspectRatio = sl::INVALID_FLOAT;
    float motionVectorsInvalidValue = sl::INVALID_FLOAT;

    bool depthInverted = false;
    bool cameraMotionIncluded = true;
    bool motionVectors3D = false;
    bool motionVectorsJittered = false;
    bool motionVectorsDilated = false;
    bool orthographicProjection = false;
    bool colorBuffersHDR = false;
};

enum class DLSSQualityMode {
    Performance,
    Balanced,
    Quality,
    UltraPerformance,
    DLAA
};

// Vulkan requires more than a VkImage handle when describing a resource to Streamline.
// Populate these from the image actually used at the DLSS evaluation point.
struct DLSSImageBinding {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE; // nullptr for swapchain-owned images
    VkImageView view = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkImageCreateFlags flags = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
};

struct DLSSResourceBindings {
    DLSSImageBinding colorBuffer;
    DLSSImageBinding depthBuffer;
    DLSSImageBinding motionVectorBuffer;
    DLSSImageBinding outputUpscaledBuffer;
};

class DLSSManager {
public:
    DLSSManager();
    ~DLSSManager();

    bool InitializeStreamline();
    bool AttachVulkan(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

    const sl::FeatureRequirements& GetRequirements() const { return dlssRequirements; }

    void Shutdown();
    bool IsDLSSSupported() const { return isSupported; }
    bool GetOptimalRenderResolution(
        uint32_t outputWidth,
        uint32_t outputHeight,
        DLSSQualityMode qualityMode,
        uint32_t& renderWidth,
        uint32_t& renderHeight);

    // Call once per rendered frame. The same acquired frame token is reused by
    // TagResources and EvaluateDLSS for that frame.
    bool UpdateFrameConstants(const DLSSFrameParams& params, uint32_t frameIndex);
    bool CheckAdapterSupport(VkPhysicalDevice physicalDevice);
    bool TagResources(VkCommandBuffer cmdBuffer, const DLSSResourceBindings& resources);
    bool EvaluateDLSS(VkCommandBuffer cmdBuffer);

private:
    VkDevice Device = VK_NULL_HANDLE;
    sl::FeatureRequirements dlssRequirements{};
    bool streamlineInitialized = false;
    bool isSupported = false;
    bool hasLoggedSuccessfulEvaluation = false;
    sl::FrameToken* currentFrameToken = nullptr;

    sl::DLSSMode GetDLSSMode(DLSSQualityMode mode) const;
};
