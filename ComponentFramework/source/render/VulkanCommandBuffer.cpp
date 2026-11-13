#include "render/VulkanRenderer.h"

void VulkanRenderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &primaryCommandBuffer.commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}


void VulkanRenderer::CreateCommandBuffers() {
    primaryCommandBuffer.commandBuffers.resize(numSwapchains);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = primaryCommandBuffer.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)primaryCommandBuffer.commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, primaryCommandBuffer.commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}
// command buffer is a list of instructions 
void VulkanRenderer::RecordCommandBuffers(Recording start_stop)
{
    if (start_stop == Recording::START)
    {
        vkWaitForFences(
            device, 1,
            &inFlightFences[currentFrame],
            VK_TRUE, UINT64_MAX
        );

        VkResult result = vkAcquireNextImageKHR(
            device,
            swapChain,
            UINT64_MAX,
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            &imageIndex
        );

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("failed to acquire swapchain image!");

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            vkWaitForFences(
                device, 1,
                &imagesInFlight[imageIndex],
                VK_TRUE, UINT64_MAX
            );
        }

        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        VkCommandBuffer cmd = primaryCommandBuffer.commandBuffers[imageIndex];

        if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS)
            throw std::runtime_error("failed to reset command buffer!");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer!");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount =
            static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(
            cmd,
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );
    }
    else if (start_stop == Recording::STOP)
    {
        VkCommandBuffer cmd = primaryCommandBuffer.commandBuffers[imageIndex];

        vkCmdEndRenderPass(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            throw std::runtime_error("failed to end command buffer!");
    }
}


void VulkanRenderer::DestroyCommandBuffers(){
    vkFreeCommandBuffers(device, primaryCommandBuffer.commandPool, 1, primaryCommandBuffer.commandBuffers.data());
    vkDestroyCommandPool(device, primaryCommandBuffer.commandPool, nullptr);  
}