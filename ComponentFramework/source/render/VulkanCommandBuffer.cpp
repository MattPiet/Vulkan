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
void VulkanRenderer::RecordCommandBuffers(Recording start_stop) {
    if (start_stop == Recording::START) {
        if (frameAcquired) {
            throw std::logic_error("A frame is already acquired and recording has started.");
        }

        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        const VkResult acquireResult = vkAcquireNextImageKHR(
            device, swapChain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
            &acquiredImageIndex);

        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        if (acquiredImageIndex >= primaryCommandBuffer.commandBuffers.size() ||
            acquiredImageIndex >= imagesInFlight.size() ||
            (upscaler_ == DLSS && acquiredImageIndex >= dlssSceneFramebuffers.size()) ||
            (upscaler_ != DLSS && acquiredImageIndex >= swapChainFramebuffers.size())) {
            throw std::runtime_error("Acquired swapchain image index is out of range.");
        }

        if (imagesInFlight[acquiredImageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[acquiredImageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[acquiredImageIndex] = inFlightFences[currentFrame];

        const VkCommandBuffer commandBuffer = primaryCommandBuffer.commandBuffers[acquiredImageIndex];
        if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS) {
            throw std::runtime_error("failed to reset command buffer!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        frameAcquired = true;
        dlssFrameConstantsReady = false;
        if (upscaler_ == DLSS && dlss_manager) {
            dlssFrameConstantsReady = dlss_manager->UpdateFrameConstants(
                dlssFrameParams, dlssFrameIndex++);
            if (!dlssFrameConstantsReady) {
                std::cerr << "[DLSS] Frame constants failed; this frame will use the low-resolution scene image as fallback.\n";
            }
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = (upscaler_ == DLSS)
            ? dlssSceneFramebuffers[acquiredImageIndex]
            : swapChainFramebuffers[acquiredImageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = (upscaler_ == DLSS) ? sceneRenderExtent : swapChainExtent;

        std::array<VkClearValue, 3> clearValues{};
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        if (upscaler_ == DLSS) {
            clearValues[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[2].depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount = 3;
        } else {
            clearValues[1].depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount = 2;
        }
        renderPassInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else if (start_stop == Recording::STOP) {
        if (!frameAcquired) {
            throw std::logic_error("Cannot stop recording because no frame is acquired.");
        }

        const VkCommandBuffer commandBuffer = primaryCommandBuffer.commandBuffers[acquiredImageIndex];
        vkCmdEndRenderPass(commandBuffer);

        if (upscaler_ == DLSS && dlss_manager) {
            auto& color = dlssColorTargets[acquiredImageIndex];
            auto& depth = dlssDepthTargets[acquiredImageIndex];
            auto& motion = dlssMotionTargets[acquiredImageIndex];
            auto& output = dlssOutputTargets[acquiredImageIndex];

            // Make the scene attachments visible to Streamline's evaluation work.
            VkImageMemoryBarrier colorReady{};
            colorReady.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            colorReady.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            colorReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            colorReady.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            colorReady.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            colorReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            colorReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            colorReady.image = color.image;
            colorReady.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &colorReady);

            VkImageMemoryBarrier depthReady{};
            depthReady.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthReady.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            depthReady.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthReady.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthReady.image = depth.image;
            depthReady.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &depthReady);

            // Motion vectors were written as the second color attachment in the scene pass.
            // Keep GENERAL as the image layout and make color-attachment writes visible to DLSS.
            VkImageMemoryBarrier motionReady{};
            motionReady.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            motionReady.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            motionReady.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            motionReady.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            motionReady.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            motionReady.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            motionReady.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            motionReady.image = motion.image;
            motionReady.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &motionReady);

            // Output starts undefined each frame because its prior contents are discarded.
            VkImageMemoryBarrier outputToGeneral{};
            outputToGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            outputToGeneral.srcAccessMask = 0;
            outputToGeneral.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            outputToGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            outputToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            outputToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            outputToGeneral.image = output.image;
            outputToGeneral.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &outputToGeneral);

            bool evaluated = false;
            if (dlssFrameConstantsReady) {
                DLSSResourceBindings bindings{};
                bindings.colorBuffer = color;
                bindings.depthBuffer = depth;
                bindings.motionVectorBuffer = motion;
                bindings.outputUpscaledBuffer = output;
                if (dlss_manager->TagResources(commandBuffer, bindings)) {
                    evaluated = dlss_manager->EvaluateDLSS(commandBuffer);
                }
            }

            // Whether DLSS succeeds or not, show a frame. On failure, linearly
            // blit the rendered lower-resolution color image as a safe fallback.
            DLSSImageBinding& source = evaluated ? output : color;
            VkImageMemoryBarrier sourceToTransfer{};
            sourceToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            sourceToTransfer.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
            sourceToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceToTransfer.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            sourceToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            sourceToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sourceToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sourceToTransfer.image = source.image;
            sourceToTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &sourceToTransfer);

            VkImageMemoryBarrier swapchainToTransfer{};
            swapchainToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainToTransfer.srcAccessMask = 0;
            swapchainToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            swapchainToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapchainToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapchainToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapchainToTransfer.image = swapChainImages[acquiredImageIndex];
            swapchainToTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainToTransfer);

            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { static_cast<int32_t>(source.width), static_cast<int32_t>(source.height), 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { static_cast<int32_t>(swapChainExtent.width), static_cast<int32_t>(swapChainExtent.height), 1 };
            vkCmdBlitImage(commandBuffer, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                swapChainImages[acquiredImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            VkImageMemoryBarrier swapchainToPresent{};
            swapchainToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapchainToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            swapchainToPresent.dstAccessMask = 0;
            swapchainToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapchainToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapchainToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapchainToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapchainToPresent.image = swapChainImages[acquiredImageIndex];
            swapchainToPresent.subresourceRange = swapchainToTransfer.subresourceRange;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainToPresent);
        }

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}


void VulkanRenderer::DestroyCommandBuffers() {
    if (!primaryCommandBuffer.commandBuffers.empty()) {
        vkFreeCommandBuffers(
            device,
            primaryCommandBuffer.commandPool,
            static_cast<uint32_t>(primaryCommandBuffer.commandBuffers.size()),
            primaryCommandBuffer.commandBuffers.data());
        primaryCommandBuffer.commandBuffers.clear();
    }

    if (primaryCommandBuffer.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, primaryCommandBuffer.commandPool, nullptr);
        primaryCommandBuffer.commandPool = VK_NULL_HANDLE;
    }
}
