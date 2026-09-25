class IUpscaler {
public:
    virtual ~IUpscaler() = default;
    
    virtual bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, const wchar_t* dataPath) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsSupported() const = 0;
    
    // The unified evaluation call your renderer will use every frame
    virtual void Evaluate(VkCommandBuffer cmdBuffer, const UpscaleParameters& params) = 0;
};