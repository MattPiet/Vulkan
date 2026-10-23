#include <cstddef>
#include "render/VulkanRenderer.h"
#include "render/DescriptorSetBuilder.h"
#include <Upscalers/DLSS/DLSSManager.h>
#include "core/Debug.h"
#include <cmath>

namespace {

sl::float4x4 ToSLRowMajor(const Matrix4& matrix)
{
    // Matrix4 is stored column-major in this engine; Streamline expects row-major.
    sl::float4x4 result{};
    for (uint32_t row = 0; row < 4; ++row) {
    for (uint32_t col = 0; col < 4; ++col) {
        const float value = matrix[col * 4 + row];
        switch (col) {
        case 0: result[row].x = value; break;
        case 1: result[row].y = value; break;
        case 2: result[row].z = value; break;
        case 3: result[row].w = value; break;
        }
    }}
    return result;
}

sl::float4x4 SLIdentityMatrix()
{
    sl::float4x4 result{};
    for (uint32_t row = 0; row < 4; ++row) {
        result[row] = sl::float4(row == 0 ? 1.0f : 0.0f,
                                 row == 1 ? 1.0f : 0.0f,
                                 row == 2 ? 1.0f : 0.0f,
                                 row == 3 ? 1.0f : 0.0f);
    }
    return result;
}

} // namespace


VulkanRenderer::VulkanRenderer(): /// Initialize all the variables
    window(nullptr), instance(nullptr), debugMessenger(0), surface(0),device(nullptr),
    windowWidth(0), windowHeight(0),presentQueue(0),graphicsQueue(nullptr), renderPass(0), swapChain(0),
    swapChainExtent{},swapChainImageFormat{}, depthImage(0), depthImageMemory(0),depthImageView(0),
    numSwapchains(0), currentFrame(0), physicalDevice(VK_NULL_HANDLE), MAX_FRAMES_IN_FLIGHT(2), sdlEvent{} {
     
 }

VulkanRenderer::~VulkanRenderer() {

}

SDL_Window* VulkanRenderer::CreateWindow(std::string name_, int width_, int height_) {
    windowWidth = width_;
    windowHeight = height_;

    // Initialize Streamline before SDL/Vulkan startup.
    dlss_manager = std::make_unique<DLSSManager>();

    streamlineInitialized =
        dlss_manager->InitializeStreamline();

    if (!streamlineInitialized)
    {
        std::cerr << "[Streamline] Initialization failed. "
                  << "Continuing without DLSS.\n";
    }

    if (SDL_Init(SDL_INIT_VIDEO) == false)
    {
        Debug::FatalError(SDL_GetError(), __FILE__, __LINE__);
        throw std::runtime_error("failed to INIT SDL");
    }

    window = SDL_CreateWindow(
        name_.c_str(),
        windowWidth,
        windowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    return window;
}

bool VulkanRenderer::OnCreate(){
    // Streamline was initialized in CreateWindow(), before SDL/Vulkan startup.
    CreateInstance();
    setupDebugMessenger();

    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
    {
        Debug::FatalError(SDL_GetError(), __FILE__, __LINE__);
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    pickPhysicalDevice();

    // Decide whether DLSS is active BEFORE logical-device creation so its
    // device extensions/features are not imposed on unsupported adapters.
    const bool dlssAdapterSupported =
        streamlineInitialized && dlss_manager &&
        dlss_manager->CheckAdapterSupport(physicalDevice);

    VkPhysicalDeviceProperties selectedGpuProperties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &selectedGpuProperties);
    const bool supportsDLSSPushConstants =
        selectedGpuProperties.limits.maxPushConstantsSize >= sizeof(ModelMatrixPushConst);

    if (dlssAdapterSupported && !supportsDLSSPushConstants) {
        std::cerr << "[DLSS] Disabled: GPU push-constant limit ("
                  << selectedGpuProperties.limits.maxPushConstantsSize
                  << " bytes) is below the DLSS motion-vector requirement ("
                  << sizeof(ModelMatrixPushConst) << " bytes).\n";
    }

    upscaler_ = dlssAdapterSupported && supportsDLSSPushConstants ? DLSS : NONE;
    
    if (upscaler_ != DLSS && dlss_manager)
    {
        dlss_manager->Shutdown();
        dlss_manager.reset();
        streamlineInitialized = false;
    }

    createLogicalDevice();

    if (upscaler_ == DLSS && dlss_manager)
    {
        if (dlss_manager->AttachVulkan(instance, physicalDevice, device))
        {
            std::cout << "[DLSS] Initialization succeeded.\n";
        }
        else
        {
            std::cerr << "[DLSS] Vulkan attachment failed; using normal rendering.\n";
            disableDLSS("Vulkan attachment failed");
        }
    }
    else
    {
        std::cout << "[DLSS] Disabled; using normal rendering. Streamline initialized: "
                  << streamlineInitialized
                  << ", adapter supported: " << dlssAdapterSupported << '\n';
    }

    createSwapChain();
    createImageViews();

    if (upscaler_ == DLSS && !createDLSSRenderTargets())
    {
        disableDLSS("could not create DLSS render targets");
        sceneRenderExtent = swapChainExtent;
    }
    else if (upscaler_ != DLSS)
    {
        sceneRenderExtent = swapChainExtent;
    }

    createRenderPass();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    CreateCommandBuffers();
    createSyncObjects();
    return true;
}

void VulkanRenderer::OnDestroy() {
    vkDeviceWaitIdle(device); /// Wait for all commands to clear
    if (dlss_manager)
    {
        dlss_manager->Shutdown();
        dlss_manager.reset();
    }
    DestroyCommandBuffers();
    destroySwapChain();

    for (VkSemaphore semaphore : renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
    }
    for (VkSemaphore semaphore : imageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
    }
    for (VkFence fence : inFlightFences) {
        if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
    }

    if (enableValidationLayers) {
        destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    window = nullptr;
    

}

void VulkanRenderer::RecreateSwapChain() {
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);

    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(&sdlEvent);
    }

    windowWidth = static_cast<uint32_t>(width);
    windowHeight = static_cast<uint32_t>(height);

    vkDeviceWaitIdle(device);
    frameAcquired = false;
    dlssFrameConstantsReady = false;
    hasPreviousDLSSViewProjection = false;

    // Render-finished semaphores are indexed by swapchain image, not by the
    // CPU frame-in-flight slot. Recreate them when the swapchain is recreated.
    for (VkSemaphore semaphore : renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
    }
    renderFinishedSemaphores.clear();

    DestroyCommandBuffers();
    destroySwapChain();
    createSwapChain();
    createImageViews();

    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        renderFinishedSemaphores.resize(swapChainImages.size(), VK_NULL_HANDLE);
        for (auto& semaphore : renderFinishedSemaphores) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
                throw std::runtime_error("failed to recreate render-finished semaphores!");
            }
        }
    }

    if (upscaler_ == DLSS && !createDLSSRenderTargets())
    {
        disableDLSS("could not recreate DLSS render targets after resize");
        sceneRenderExtent = swapChainExtent;
    }
    else if (upscaler_ != DLSS)
    {
        sceneRenderExtent = swapChainExtent;
    }

    createRenderPass();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    CreateCommandBuffers();
    imagesInFlight.assign(swapChainImages.size(), VK_NULL_HANDLE);
}


void VulkanRenderer::Render() {
    if (!frameAcquired) {
        throw std::logic_error("Render called before RecordCommandBuffers(START).");
    }

    const uint32_t imageIndex = acquiredImageIndex;
    const VkCommandBuffer commandBuffer = primaryCommandBuffer.commandBuffers[imageIndex];

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { (upscaler_ == DLSS)
        ? VK_PIPELINE_STAGE_TRANSFER_BIT
        : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // A presentation wait may outlive the frame fence. Reuse this semaphore only
    // when the same swapchain image is acquired again.
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[acquiredImageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        frameAcquired = false;
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &acquiredImageIndex;

    const VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
    frameAcquired = false;

    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::DestroyUBO(std::vector<BufferMemory> ubo){
     for (size_t i = 0; i < ubo.size(); i++) {
        vkDestroyBuffer(device, ubo[i].bufferID, nullptr);
        vkFreeMemory(device, ubo[i].bufferMemoryID, nullptr);
     }
}

void VulkanRenderer::destroySwapChain() {
    for (auto framebuffer : dlssSceneFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    dlssSceneFramebuffers.clear();

    for (auto framebuffer : swapChainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    auto destroyImageBindings = [this](std::vector<DLSSImageBinding>& images) {
        for (auto& image : images) {
            if (image.view != VK_NULL_HANDLE) vkDestroyImageView(device, image.view, nullptr);
            if (image.image != VK_NULL_HANDLE) vkDestroyImage(device, image.image, nullptr);
            if (image.memory != VK_NULL_HANDLE) vkFreeMemory(device, image.memory, nullptr);
        }
        images.clear();
    };
    destroyImageBindings(dlssColorTargets);
    destroyImageBindings(dlssDepthTargets);
    destroyImageBindings(dlssMotionTargets);
    destroyImageBindings(dlssOutputTargets);

    if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, nullptr);
    if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
    if (depthImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthImageMemory, nullptr);
    depthImageView = VK_NULL_HANDLE;
    depthImage = VK_NULL_HANDLE;
    depthImageMemory = VK_NULL_HANDLE;

    for (auto imageView : swapChainImageViews) {
        if (imageView != VK_NULL_HANDLE) vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();
    swapChainImages.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}



void VulkanRenderer::CreateInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
       throw std::runtime_error("validation layers requested, but not available!");
    }
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Game 308";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    
    if (streamlineInitialized && dlss_manager)
    {
        const auto& req = dlss_manager->GetRequirements();
        for (uint32_t i = 0; i < req.vkNumInstanceExtensions; ++i)
        {
            const char* required = req.vkInstanceExtensions[i];
            const bool alreadyAdded = std::any_of(
                extensions.begin(), extensions.end(),
                [required](const char* existing) { return std::strcmp(existing, required) == 0; });
            if (!alreadyAdded) extensions.push_back(required);
        }
    }
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    const VkResult instanceResult = vkCreateInstance(&createInfo, nullptr, &instance);
    if (instanceResult != VK_SUCCESS) {
        // Streamline is optional. If its extra instance extensions prevent Vulkan
        // instance creation, shut Streamline down and retry the base renderer.
        if (streamlineInitialized && dlss_manager) {
            std::cerr << "[DLSS] Vulkan instance creation with Streamline requirements failed; retrying without DLSS.\n";
            dlss_manager->Shutdown();
            dlss_manager.reset();
            streamlineInitialized = false;

            auto baseExtensions = getRequiredExtensions();
            createInfo.enabledExtensionCount = static_cast<uint32_t>(baseExtensions.size());
            createInfo.ppEnabledExtensionNames = baseExtensions.data();
            if (vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS) {
                std::cout << "[Vulkan] Instance created without Streamline extensions.\n";
                return;
            }
        }
        throw std::runtime_error("failed to create Vulkan instance!");
    }
}



void VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}



void VulkanRenderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice fallbackDevice = VK_NULL_HANDLE;

    for (const auto& d : devices) {
        if (isDeviceSuitable(d)) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(d, &deviceProperties);

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physicalDevice = d;
                break;
            }
            else if (fallbackDevice == VK_NULL_HANDLE) {
                fallbackDevice = d;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE && fallbackDevice != VK_NULL_HANDLE) {
        physicalDevice = fallbackDevice;
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    // Fixed: properly query properties before printing
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "Selected GPU for Vulkan: " << props.deviceName << std::endl;
}

void VulkanRenderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    const bool useDLSS = (upscaler_ == DLSS && dlss_manager && streamlineInitialized);

    // DLSS uses a second color attachment for motion vectors. The scene-color
    // and motion-vector attachments have different colorWriteMask values, so
    // Vulkan's independentBlend feature is required for that pipeline.
    // Query support first and fall back to NONE if the adapter lacks it.
    VkPhysicalDeviceFeatures supportedFeatures{};
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
    if (useDLSS && supportedFeatures.independentBlend != VK_TRUE) {
        disableDLSS("GPU does not support independentBlend required by the motion-vector pipeline");
        createLogicalDevice();
        return;
    }

    VkPhysicalDeviceFeatures enabledFeatures{};
    enabledFeatures.samplerAnisotropy = VK_TRUE;
    enabledFeatures.independentBlend = useDLSS ? VK_TRUE : VK_FALSE;
    // The project's current non-DLSS shaders do not use these features.
    // Do not make them requirements for the NONE/AMD fallback path.
    enabledFeatures.shaderStorageImageWriteWithoutFormat = useDLSS ? VK_TRUE : VK_FALSE;
    enabledFeatures.shaderInt16 = useDLSS ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceVulkan12Features features12{};
    VkPhysicalDeviceVulkan11Features features11{};
    // Streamline uses Vulkan private-data slots during slSetVulkanInfo.
    // This is an extension feature so it can be enabled without requiring the
    // base renderer to request Vulkan 1.3.
    VkPhysicalDevicePrivateDataFeaturesEXT privateDataFeatures{};
    VkPhysicalDeviceFeatures2 deviceFeatures2{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &enabledFeatures;
    createInfo.pNext = nullptr;

    // Keep the base renderer's extension list minimal. This project uses regular
    // descriptor sets, not VK_KHR_push_descriptor. Streamline-required extensions
    // are appended below only when upscaler_ == DLSS.
    std::vector<const char*> activeExtensions = deviceExtensions;

    if (useDLSS) {
        const auto& req = dlss_manager->GetRequirements();
        features12 = sl::getVkPhysicalDeviceVulkan12Features(req.vkNumFeatures12, req.vkFeatures12);
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        // Keep the features this project previously required for its Streamline path.
        features12.bufferDeviceAddress = VK_TRUE;
        features12.timelineSemaphore = VK_TRUE;
        features12.vulkanMemoryModel = VK_TRUE;
        features12.vulkanMemoryModelDeviceScope = VK_TRUE;
        features12.shaderFloat16 = VK_TRUE;
        features12.shaderInt8 = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        // Streamline's Vulkan attachment calls vkCreatePrivateDataSlot().
        // Enabling the feature is required; loading the function alone is not enough.
        privateDataFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT;
        privateDataFeatures.privateData = VK_TRUE;
        features12.pNext = &privateDataFeatures;

        features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features11.storageBuffer16BitAccess = VK_TRUE;
        features11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
        features11.pNext = &features12;

        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.features = enabledFeatures;
        deviceFeatures2.pNext = &features11;
        createInfo.pEnabledFeatures = nullptr;
        createInfo.pNext = &deviceFeatures2;

        for (uint32_t i = 0; i < req.vkNumDeviceExtensions; ++i) {
            const char* required = req.vkDeviceExtensions[i];
            const bool alreadyAdded = std::any_of(
                activeExtensions.begin(), activeExtensions.end(),
                [required](const char* existing) { return std::strcmp(existing, required) == 0; });
            if (!alreadyAdded) activeExtensions.push_back(required);
        }

        // VK_EXT_private_data is promoted to Vulkan 1.3, but enabling its
        // extension path lets this renderer keep its Vulkan 1.2 instance API.
        const bool hasPrivateDataExtension = std::any_of(
            activeExtensions.begin(), activeExtensions.end(),
            [](const char* existing) {
                return std::strcmp(existing, VK_EXT_PRIVATE_DATA_EXTENSION_NAME) == 0;
            });
        if (!hasPrivateDataExtension) {
            activeExtensions.push_back(VK_EXT_PRIVATE_DATA_EXTENSION_NAME);
        }

        // Vulkan 1.2 core features supersede this extension; do not enable both.
        activeExtensions.erase(
            std::remove_if(activeExtensions.begin(), activeExtensions.end(), [](const char* extension) {
                return std::strcmp(extension, VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0;
            }),
            activeExtensions.end());
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(activeExtensions.size());
    createInfo.ppEnabledExtensionNames = activeExtensions.data();
    createInfo.enabledLayerCount = 0;

    const VkResult deviceResult = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    if (deviceResult != VK_SUCCESS) {
        device = VK_NULL_HANDLE;
        if (useDLSS) {
            // Keep the ordinary renderer usable if optional DLSS requirements cannot
            // be enabled on this adapter/driver. Retry with the NONE configuration.
            disableDLSS("logical-device creation with DLSS requirements failed");
            createLogicalDevice();
            return;
        }
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}


void VulkanRenderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    /// This generates the minimum number of images (swapchains) for this device.
    /// I'm just going to make one more if I can
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    numSwapchains = imageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (upscaler_ == DLSS) {
        const VkImageUsageFlags requiredUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if ((swapChainSupport.capabilities.supportedUsageFlags & requiredUsage) == requiredUsage) {
            createInfo.imageUsage = requiredUsage;
        } else {
            disableDLSS("surface does not support transfer-destination swapchain images");
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    numSwapchains = imageCount;
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    if (upscaler_ != DLSS) sceneRenderExtent = swapChainExtent;
}

void VulkanRenderer::createImageViews() {
    swapChainImageViews.resize(numSwapchains);

    for (uint32_t i = 0; i < numSwapchains; i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanRenderer::createRenderPass() {
    const bool useDLSS = (upscaler_ == DLSS);

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = useDLSS ? DLSS_COLOR_FORMAT : swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = useDLSS
        ? VK_IMAGE_LAYOUT_GENERAL
        : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription motionAttachment{};
    motionAttachment.format = DLSS_MOTION_FORMAT;
    motionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    motionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    motionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    motionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    motionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    motionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    motionAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = useDLSS ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = useDLSS
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference motionAttachmentRef{};
    motionAttachmentRef.attachment = 1;
    motionAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = useDLSS ? 2 : 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    const std::array<VkAttachmentReference, 2> colorAttachmentRefs = {
        colorAttachmentRef, motionAttachmentRef
    };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = useDLSS ? 2u : 1u;
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments;
    attachments.push_back(colorAttachment);
    if (useDLSS) attachments.push_back(motionAttachment);
    attachments.push_back(depthAttachment);

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0] = dependency;
    uint32_t dependencyCount = 1;
    if (useDLSS) {
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencyCount = 2;
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = dependencyCount;
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}


void VulkanRenderer::createDepthResources() {
    const VkFormat depthFormat = findDepthFormat();

    if (upscaler_ == DLSS) {
        dlssDepthTargets.resize(numSwapchains);
        for (auto& target : dlssDepthTargets) {
            target.width = sceneRenderExtent.width;
            target.height = sceneRenderExtent.height;
            target.format = depthFormat;
            target.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            target.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            createImage(target.width, target.height, target.format, VK_IMAGE_TILING_OPTIMAL,
                target.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, target.image, target.memory);
            target.view = createImageView(target.image, target.format, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        return;
    }

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}


VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

VkFormat VulkanRenderer::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
            ((upscaler_ == DLSS) ? VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT : 0)
    );
}

bool VulkanRenderer::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanRenderer::createFramebuffers() {
    if (upscaler_ == DLSS) {
        dlssSceneFramebuffers.resize(numSwapchains, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < numSwapchains; ++i) {
            const std::array<VkImageView, 3> attachments = {
                dlssColorTargets[i].view,
                dlssMotionTargets[i].view,
                dlssDepthTargets[i].view
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = sceneRenderExtent.width;
            framebufferInfo.height = sceneRenderExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &dlssSceneFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create DLSS scene framebuffer!");
            }
        }
        return;
    }

    swapChainFramebuffers.resize(numSwapchains, VK_NULL_HANDLE);
    for (size_t i = 0; i < numSwapchains; i++) {
        const std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}


bool VulkanRenderer::checkDLSSFormatSupport() const {
    VkFormatProperties colorProps{};
    VkFormatProperties motionProps{};
    VkFormatProperties outputProps{};
    VkFormatProperties swapProps{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, DLSS_COLOR_FORMAT, &colorProps);
    vkGetPhysicalDeviceFormatProperties(physicalDevice, DLSS_MOTION_FORMAT, &motionProps);
    vkGetPhysicalDeviceFormatProperties(physicalDevice, DLSS_OUTPUT_FORMAT, &outputProps);
    vkGetPhysicalDeviceFormatProperties(physicalDevice, swapChainImageFormat, &swapProps);

    const VkFormatFeatureFlags colorRequired =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
        VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    const VkFormatFeatureFlags motionRequired =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
        VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    const VkFormatFeatureFlags outputRequired =
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    bool depthFormatFound = false;
    for (VkFormat candidate : { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }) {
        VkFormatProperties depthProps{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, candidate, &depthProps);
        const VkFormatFeatureFlags depthRequired =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((depthProps.optimalTilingFeatures & depthRequired) == depthRequired) {
            depthFormatFound = true;
            break;
        }
    }

    return depthFormatFound &&
           (colorProps.optimalTilingFeatures & colorRequired) == colorRequired &&
           (motionProps.optimalTilingFeatures & motionRequired) == motionRequired &&
           (outputProps.optimalTilingFeatures & outputRequired) == outputRequired &&
           (swapProps.optimalTilingFeatures &
               (VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) ==
               (VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
}

bool VulkanRenderer::createDLSSRenderTargets() {
    if (upscaler_ != DLSS || !dlss_manager || !dlss_manager->IsDLSSSupported()) return false;
    if (!checkDLSSFormatSupport()) {
        std::cerr << "[DLSS] Required image formats are not supported by this Vulkan device.\n";
        return false;
    }

    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    if (!dlss_manager->GetOptimalRenderResolution(
            swapChainExtent.width, swapChainExtent.height,
            DLSSQualityMode::Quality, renderWidth, renderHeight)) {
        std::cerr << "[DLSS] Failed to obtain the recommended render resolution.\n";
        return false;
    }

    sceneRenderExtent = { renderWidth, renderHeight };
    std::cout << "[DLSS] Render resolution: " << sceneRenderExtent.width << "x"
              << sceneRenderExtent.height << " -> " << swapChainExtent.width << "x"
              << swapChainExtent.height << '\n';

    destroyDLSSRenderTargets();
    try {
        dlssColorTargets.resize(numSwapchains);
        dlssMotionTargets.resize(numSwapchains);
        dlssOutputTargets.resize(numSwapchains);

        for (uint32_t i = 0; i < numSwapchains; ++i) {
            auto& color = dlssColorTargets[i];
            color.width = renderWidth;
            color.height = renderHeight;
            color.format = DLSS_COLOR_FORMAT;
            color.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            color.layout = VK_IMAGE_LAYOUT_GENERAL;
            createImage(color.width, color.height, color.format, VK_IMAGE_TILING_OPTIMAL,
                color.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color.image, color.memory);
            color.view = createImageView(color.image, color.format, VK_IMAGE_ASPECT_COLOR_BIT);

            auto& motion = dlssMotionTargets[i];
            motion.width = renderWidth;
            motion.height = renderHeight;
            motion.format = DLSS_MOTION_FORMAT;
            // Streamline frame-based resource tagging may copy this volatile input.
            // It therefore needs both transfer-source and transfer-destination usage.
            motion.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            motion.layout = VK_IMAGE_LAYOUT_GENERAL;
            createImage(motion.width, motion.height, motion.format, VK_IMAGE_TILING_OPTIMAL,
                motion.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, motion.image, motion.memory);
            motion.view = createImageView(motion.image, motion.format, VK_IMAGE_ASPECT_COLOR_BIT);

            auto& output = dlssOutputTargets[i];
            output.width = swapChainExtent.width;
            output.height = swapChainExtent.height;
            output.format = DLSS_OUTPUT_FORMAT;
            // Streamline may clear/init the output before writing it. Include
            // TRANSFER_DST as well as storage and transfer-source usage.
            output.usage = VK_IMAGE_USAGE_STORAGE_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            output.layout = VK_IMAGE_LAYOUT_GENERAL;
            createImage(output.width, output.height, output.format, VK_IMAGE_TILING_OPTIMAL,
                output.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, output.image, output.memory);
            output.view = createImageView(output.image, output.format, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }
    catch (...) {
        destroyDLSSRenderTargets();
        sceneRenderExtent = swapChainExtent;
        return false;
    }
    return true;
}

void VulkanRenderer::destroyDLSSRenderTargets() {
    auto destroyImages = [this](std::vector<DLSSImageBinding>& images) {
        for (auto& target : images) {
            if (target.view != VK_NULL_HANDLE) vkDestroyImageView(device, target.view, nullptr);
            if (target.image != VK_NULL_HANDLE) vkDestroyImage(device, target.image, nullptr);
            if (target.memory != VK_NULL_HANDLE) vkFreeMemory(device, target.memory, nullptr);
            target = DLSSImageBinding{};
        }
        images.clear();
    };
    destroyImages(dlssColorTargets);
    destroyImages(dlssDepthTargets);
    destroyImages(dlssMotionTargets);
    destroyImages(dlssOutputTargets);
}

void VulkanRenderer::disableDLSS(const char* reason) {
    std::cerr << "[DLSS] Disabled; using normal rendering: " << reason << '\n';
    upscaler_ = NONE;
    dlssFrameConstantsReady = false;
    hasPreviousDLSSViewProjection = false;
    if (dlss_manager) {
        dlss_manager->Shutdown();
        dlss_manager.reset();
    }
    streamlineInitialized = false;
}

void VulkanRenderer::SetDLSSCameraMatrices(const Matrix4& projection, const Matrix4& view) {
    if (!IsDLSSActive()) return;

    dlssFrameParams.renderWidth = sceneRenderExtent.width;
    dlssFrameParams.renderHeight = sceneRenderExtent.height;
    dlssFrameParams.displayWidth = swapChainExtent.width;
    dlssFrameParams.displayHeight = swapChainExtent.height;
    dlssFrameParams.qualityMode = DLSSQualityMode::Balanced;
    dlssFrameParams.sharpness = 0.0f;
    dlssFrameParams.jitterX = 0.0f;
    dlssFrameParams.jitterY = 0.0f;
    dlssFrameParams.depthInverted = false;
    dlssFrameParams.cameraMotionIncluded = true;
    dlssFrameParams.motionVectors3D = false;
    dlssFrameParams.motionVectorsJittered = false;
    dlssFrameParams.motionVectorsDilated = false;
    dlssFrameParams.orthographicProjection = false;
    dlssFrameParams.colorBuffersHDR = false;
    // DLSS shader variants output normalized previous-UV minus current-UV vectors.
    dlssFrameParams.motionVectorScale = { 1.0f, 1.0f };
    dlssFrameParams.cameraNear = 0.5f;
    dlssFrameParams.cameraFar = 100.0f;
    dlssFrameParams.cameraAspectRatio = static_cast<float>(swapChainExtent.width) /
                                        static_cast<float>(std::max(1u, swapChainExtent.height));
    dlssFrameParams.cameraFOV = (std::fabs(projection[5]) > 0.00001f)
        ? 2.0f * std::atan(1.0f / std::fabs(projection[5]))
        : 0.78539816339f;

    dlssFrameParams.viewToClipMatrix = ToSLRowMajor(projection);
    dlssFrameParams.inverseViewToClip = ToSLRowMajor(MMath::inverse(projection));

    const Matrix4 currentViewProjection = projection * view;
    if (hasPreviousDLSSViewProjection) {
        previousDLSSProjectionForFrame = lastDLSSProjection;
        previousDLSSViewForFrame = lastDLSSView;
        const Matrix4 previousViewProjection = previousDLSSProjectionForFrame * previousDLSSViewForFrame;
        dlssFrameParams.clipToPrevClipMatrix = ToSLRowMajor(
            previousViewProjection * MMath::inverse(currentViewProjection));
        dlssFrameParams.prevClipToClipMatrix = ToSLRowMajor(
            currentViewProjection * MMath::inverse(previousViewProjection));
        dlssFrameParams.isCameraCut = false;
    } else {
        // Avoid a bogus first-frame vector by making the previous camera equal current.
        previousDLSSProjectionForFrame = projection;
        previousDLSSViewForFrame = view;
        dlssFrameParams.clipToPrevClipMatrix = SLIdentityMatrix();
        dlssFrameParams.prevClipToClipMatrix = SLIdentityMatrix();
        dlssFrameParams.isCameraCut = true;
    }
    lastDLSSProjection = projection;
    lastDLSSView = view;
    hasPreviousDLSSViewProjection = true;

    const Matrix4 cameraWorld = MMath::inverse(view);
    dlssFrameParams.cameraPosition = { cameraWorld[12], cameraWorld[13], cameraWorld[14] };
    dlssFrameParams.cameraRight = { cameraWorld[0], cameraWorld[1], cameraWorld[2] };
    dlssFrameParams.cameraUp = { cameraWorld[4], cameraWorld[5], cameraWorld[6] };
    dlssFrameParams.cameraForward = { -cameraWorld[8], -cameraWorld[9], -cameraWorld[10] };
    dlssFrameParams.motionVectorsInvalidValue = sl::INVALID_FLOAT;
}


void VulkanRenderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    // Render-finished semaphores are per swapchain image because presentation
    // can still be waiting on a semaphore after the submit fence has signaled.
    renderFinishedSemaphores.resize(swapChainImages.size());
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create frame synchronization objects!");
        }
    }

    for (auto& semaphore : renderFinishedSemaphores) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render-finished semaphore!");
        }
    }
}


void VulkanRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer,sourceStage, destinationStage,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::transitionCubeImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    
    // CRITICAL DIFFERENCE: layerCount is set to 6 so all faces transition together
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6; 

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; 
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    // Record the barrier into the command buffer passed into the function
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = primaryCommandBuffer.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, primaryCommandBuffer.commandPool, 1, &commandBuffer);
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}



VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = { windowWidth, windowHeight };
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}

SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device) {
    
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) {
            break;
        }
        i++;
    }
    return indices;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() {
    uint32_t extensionCount;
    std::vector<const char*> extensions;

    const char* const* instance_extensions;
    instance_extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!instance_extensions) {
        throw std::runtime_error("failed to get required SDL extensions!");
    }

    for (int i = 0; i < extensionCount; ++i) {
        extensions.push_back(instance_extensions[i]);
    }

    if (enableValidationLayers){
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::string msg = pCallbackData->pMessage;
    std::string header;
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            header = "Validation Layer:[VERBOSE]\n ";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            header = "Validation Layer:[INFO]\n ";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            header = "Validation Layer:[WARNING]\n ";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            header = "Validation Layer:[ERROR]\n ";
            break;
        default:
            header = "Validation Layer:[UNKNOWN]\n ";
            break;
    }
   
    insertLineBreaks(msg,80);
    std::cerr << header << msg << "\n\n";
    return VK_FALSE;
}


VkResult VulkanRenderer::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        VkResult result = func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        return result;
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanRenderer::destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanRenderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void VulkanRenderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr,&surface)) {
        throw std::runtime_error("failed to create window");
    }
}

void VulkanRenderer::BindPipeline(VkPipeline pipeline) {
    const VkCommandBuffer commandBuffer = GetActiveCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        throw std::logic_error("BindPipeline called outside active command-buffer recording.");
    }
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}




/// Allocate a descriptor set per swapchain image.
/// During rendering, bind the descriptor set associated with the current swapchain image.

void VulkanRenderer::BindDescriptorSet(VkPipelineLayout pipelineLayout, int firstSet, std::vector<VkDescriptorSet> descriptorSet) {
    const VkCommandBuffer commandBuffer = GetActiveCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        throw std::logic_error("BindDescriptorSet called outside active command-buffer recording.");
    }
    if (descriptorSet.size() <= acquiredImageIndex) {
        throw std::runtime_error("Descriptor-set array does not contain the acquired swapchain image index.");
    }

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        static_cast<uint32_t>(firstSet),
        1,
        &descriptorSet[acquiredImageIndex],
        0,
        nullptr);
}

void VulkanRenderer::SetPushConstant(
    const PipelineInfo pipelineInfo,
    const Matrix4& modelMatrix_,
    const Matrix4* previousModelMatrix) {
    ModelMatrixPushConst modelMatrixPushConst{};
    modelMatrixPushConst.modelMatrix = modelMatrix_;
    modelMatrixPushConst.previousModelMatrix = previousModelMatrix ? *previousModelMatrix : modelMatrix_;
    Matrix3 normalMatrix = Matrix3(MMath::inverse(modelMatrix_));

    modelMatrixPushConst.normalMatrix[0].x = normalMatrix[0];
    modelMatrixPushConst.normalMatrix[1].x = normalMatrix[1];
    modelMatrixPushConst.normalMatrix[2].x = normalMatrix[2];
    modelMatrixPushConst.normalMatrix[0].y = normalMatrix[3];
    modelMatrixPushConst.normalMatrix[1].y = normalMatrix[4];
    modelMatrixPushConst.normalMatrix[2].y = normalMatrix[5];
    modelMatrixPushConst.normalMatrix[0].z = normalMatrix[6];
    modelMatrixPushConst.normalMatrix[1].z = normalMatrix[7];
    modelMatrixPushConst.normalMatrix[2].z = normalMatrix[8];

    const VkCommandBuffer commandBuffer = GetActiveCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        throw std::logic_error("SetPushConstant called outside active command-buffer recording.");
    }

    vkCmdPushConstants(
        commandBuffer, pipelineInfo.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0,
        (upscaler_ == DLSS)
            ? static_cast<uint32_t>(sizeof(ModelMatrixPushConst))
            : static_cast<uint32_t>(offsetof(ModelMatrixPushConst, previousModelMatrix)),
        &modelMatrixPushConst);
}



void VulkanRenderer::DestroyDescriptorSet(DescriptorSetInfo& descriptorSetInfo) {
    vkDestroyDescriptorSetLayout(device, descriptorSetInfo.descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorSetInfo.descriptorPool, nullptr);
    descriptorSetInfo.descriptorSet.clear();
}