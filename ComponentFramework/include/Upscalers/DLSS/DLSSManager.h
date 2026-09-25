#pragma once

#include <vulkan/vulkan.h>
#include <sl.h>
#include <sl_dlss.h>
#include <sl_helpers_vk.h>


enum class DLSSQualityMode;

struct DLSSFrameParams {
    uint32_t renderWidth;
    uint32_t renderHeight;
    uint32_t displayWidth;
    uint32_t displayHeight;
    float jitterX;
    float jitterY;
    float sharpness;
    bool isCameraCut;
    DLSSQualityMode qualityMode;
    sl::float4x4 viewToClipMatrix;
    sl::float4x4 inverseViewToClip;
};

struct DLSSResourceBindings {
    VkImage colorBufferImage;
    VkImage depthBufferImage;
    VkImage motionVectorImage;
    VkImage outputUpscaledImage;
};

enum class DLSSQualityMode {
    Performance,
    Balanced,
    Quality,
    UltraPerformance,
    DLAA
};

class DLSSManager {
public:
    DLSSManager();
    ~DLSSManager();

    bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t appId, const wchar_t* dataPath);
    void Shutdown();

    bool IsDLSSSupported() const { return m_isSupported; }
    void UpdateFrameConstants(const DLSSFrameParams& params, uint32_t frameIndex);
    void TagResources(VkCommandBuffer cmdBuffer, const DLSSResourceBindings& resources);
    void EvaluateDLSS(VkCommandBuffer cmdBuffer);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    bool m_isSupported = false;
    
    sl::DLSSMode GetDLSSMode(DLSSQualityMode mode) {
        switch (mode) {
        case DLSSQualityMode::Performance:      return sl::DLSSMode::eMaxPerformance;
        case DLSSQualityMode::Balanced:         return sl::DLSSMode::eBalanced;
        case DLSSQualityMode::Quality:          return sl::DLSSMode::eMaxQuality;
        case DLSSQualityMode::UltraPerformance: return sl::DLSSMode::eUltraPerformance;
        case DLSSQualityMode::DLAA:             return sl::DLSSMode::eDLAA;
        default:                                return sl::DLSSMode::eOff;
        }
    }
};