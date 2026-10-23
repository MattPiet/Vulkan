#include "render/VulkanRenderer.h"

Sampler2D VulkanRenderer::Create2DTextureImage(const char* textureFile) {

    Sampler2D texture2D;
    SDL_Surface* rawImage = IMG_Load(textureFile);
    // Convert to a guaranteed 32-bit RGBA format
    SDL_Surface* image = SDL_ConvertSurface(rawImage, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(rawImage); // Free the original 24-bit surfaceS
    VkDeviceSize imageSize = image->w * image->h * 4; /// RGBA only please

    BufferMemory stagingBuffer{};
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    // map pushes memory from the GPU into the CPU
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, imageSize, 0, &data);
    // Modify the data in the CPU memory
    memcpy(data, image->pixels, static_cast<size_t>(imageSize)); /// memcpy wants a 32 bit not a 64 bit int thus the type cast
    // unmap pushes memory back into the GPU
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);


    createImage(image->w, image->h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture2D.image, texture2D.imageDeviceMemory);

    transitionImageLayout(texture2D.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer.bufferID, texture2D.image, static_cast<uint32_t>(image->w), static_cast<uint32_t>(image->h));
    transitionImageLayout(texture2D.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
    SDL_DestroySurface(image);

    createTextureImageView(texture2D);
    createTextureSampler(texture2D);
    return texture2D;
}


/// Configure how this image is to be sampled - this is same a the openGl texture filtering
void VulkanRenderer::createTextureSampler(Sampler2D& texture2D) const {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture2D.sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void VulkanRenderer::createTextureImageView(Sampler2D& texture2D) {
    texture2D.imageView = createImageView(texture2D.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}


/// Defines the way the shader looks at the image
VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }
    return imageView;
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }
    vkBindImageMemory(device, image, imageMemory, 0);
}


void VulkanRenderer::createCubeImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6; // 6 layers for the 6 faces
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // Critical for skyboxes

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create cubemap image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate cubemap image memory!");
    }
    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView VulkanRenderer::createCubeImageView(VkImage image, VkFormat format, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; // View as a cube, not 2D
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6; // Include all 6 layers

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create cubemap image view!");
    }
    return imageView;
}

SamplerCube VulkanRenderer::CreateCubeMapTexture(const std::string faces[6]) {

    SamplerCube textureCube;
    SDL_Surface* surfaces[6];

    for (int i = 0; i < 6; i++) {
        SDL_Surface* tempSurface = IMG_Load(faces[i].c_str());
        if (!tempSurface) {
            throw std::runtime_error("Failed to load skybox face!");
        }
        // Convert surface to RGBA32 format to guarantee uniform 4-byte pixels and correct pitch
        surfaces[i] = SDL_ConvertSurface(tempSurface, SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(tempSurface);
    }

    uint32_t width = static_cast<uint32_t>(surfaces[0]->w);
    uint32_t height = static_cast<uint32_t>(surfaces[0]->h);
    VkDeviceSize layerSize = width * height * 4; // 4 bytes for RGBA
    VkDeviceSize totalSize = layerSize * 6;

    // CREATE STAGING BUFFER
    BufferMemory stagingBuffer{};
    CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    // COPY SDL PIXELS TO STAGING BUFFER
    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, totalSize, 0, &data);
    for (int i = 0; i < 6; i++) {
        memcpy(static_cast<char*>(data) + (layerSize * i), surfaces[i]->pixels, static_cast<size_t>(layerSize));
    }
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    // ALLOCATE THE CUBEMAP IMAGE ON THE GPU
    createCubeImage(width, height, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureCube.image, textureCube.imageDeviceMemory);

    // USE VULKAN COMMANDS TO MOVE PIXELS FROM STAGING BUFFER TO CUBEMAP
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    // Prepare image to receive data
    transitionCubeImageLayout(commandBuffer, textureCube.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

    // Copy the 6 faces
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    for (uint32_t i = 0; i < 6; i++) {
        VkBufferImageCopy region{};
        region.bufferOffset = layerSize * i;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i; // Tells Vulkan which face to write to
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { width, height, 1 };
        bufferCopyRegions.push_back(region);
    }

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.bufferID, textureCube.image, 
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                           static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

    // Prepare image for shaders to read it
    transitionCubeImageLayout(commandBuffer, textureCube.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

    endSingleTimeCommands(commandBuffer);

    //  CLEAN UP CPU MEMORY
    for (int i = 0; i < 6; i++) {
        SDL_DestroySurface(surfaces[i]);
    }
    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);

    // CREATE VIEWS AND SAMPLERS
    textureCube.imageView = createCubeImageView(textureCube.image, VK_FORMAT_R8G8B8A8_SRGB, 1);
    
    // Reuse your existing texture sampler code
    Sampler2D tempSampler;
    createTextureSampler(tempSampler);
    textureCube.sampler = tempSampler.sampler;

    return textureCube;
}

void VulkanRenderer::DestroySampler2D(Sampler2D& texture2D) {
    vkDestroyImageView(device, texture2D.imageView, nullptr);
    vkDestroyImage(device, texture2D.image, nullptr);
    vkFreeMemory(device, texture2D.imageDeviceMemory, nullptr);
    vkDestroySampler(device, texture2D.sampler, nullptr);  
}

void VulkanRenderer::DestroySamplerCube(SamplerCube& textureCube) {
    if (textureCube.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, textureCube.imageView, nullptr);
        textureCube.imageView = VK_NULL_HANDLE;
    }
    if (textureCube.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, textureCube.image, nullptr);
        textureCube.image = VK_NULL_HANDLE;
    }
    if (textureCube.imageDeviceMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, textureCube.imageDeviceMemory, nullptr);
        textureCube.imageDeviceMemory = VK_NULL_HANDLE;
    }
    if (textureCube.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, textureCube.sampler, nullptr);
        textureCube.sampler = VK_NULL_HANDLE;
    }
}
