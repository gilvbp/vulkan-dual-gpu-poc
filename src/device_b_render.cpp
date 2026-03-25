#include "device_a_present.h"
#include "util.h"
#include <stdexcept>

void DeviceAPresent::init(
    VkPhysicalDevice phys,
    VkDevice dev,
    uint32_t graphicsQueueFamily,
    uint32_t computeQueueFamily,
    VkQueue graphicsQueue,
    VkQueue computeQueue,
    VkQueue presentQueue)
{
    phys_ = phys;
    dev_ = dev;

    graphicsQueueFamily_ = graphicsQueueFamily;
    computeQueueFamily_ = computeQueueFamily;

    graphicsQueue_ = graphicsQueue;
    computeQueue_ = computeQueue;
    presentQueue_ = presentQueue;

    timestampsA_.init(phys_, dev_, QA_COUNT);
}

void DeviceAPresent::importSharedTargets(
    uint32_t frameCount,
    const SharedImageCreateInfo& info,
    const std::vector<ExportedImageHandle>& exported)
{
    imageInfo_ = info;
    imports_.clear();
    imports_.reserve(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        imports_.push_back(
            SharedImage::importFromFd(
                phys_,
                dev_,
                info,
                exported[i].memoryFd,
                exported[i].allocationSize));
    }
}

void DeviceAPresent::createSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    surface_ = surface;
    extent_ = { width, height };

    VkSwapchainCreateInfoKHR sci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = 2,
        .imageFormat = swapFormat_,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent_,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };

    vkCheck(vkCreateSwapchainKHR(dev_, &sci, nullptr, &swapchain_), "vkCreateSwapchainKHR");

    uint32_t count = 0;
    vkCheck(vkGetSwapchainImagesKHR(dev_, swapchain_, &count, nullptr), "vkGetSwapchainImagesKHR count");
    swapImages_.resize(count);
    vkCheck(vkGetSwapchainImagesKHR(dev_, swapchain_, &count, swapImages_.data()), "vkGetSwapchainImagesKHR data");

    VkCommandPoolCreateInfo pci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily_
    };
    vkCheck(vkCreateCommandPool(dev_, &pci, nullptr, &cmdPool_), "vkCreateCommandPool A");

    cmdBuffers_.resize(count);
    VkCommandBufferAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdPool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(cmdBuffers_.size())
    };
    vkCheck(vkAllocateCommandBuffers(dev_, &ai, cmdBuffers_.data()), "vkAllocateCommandBuffers A");

    VkSemaphoreCreateInfo sciSem{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    vkCheck(vkCreateSemaphore(dev_, &sciSem, nullptr, &acquireSemaphore_), "vkCreateSemaphore acquire");
    vkCheck(vkCreateSemaphore(dev_, &sciSem, nullptr, &renderCompleteSemaphore_), "vkCreateSemaphore renderComplete");
}

void DeviceAPresent::runComputePass(uint32_t, uint64_t) {
    // Ainda não implementado: passo de compute local na GPU A.
}

void DeviceAPresent::composeAndPresent(uint32_t slot) {
    if (slot >= imports_.size()) {
        throw std::runtime_error("composeAndPresent: slot fora do intervalo");
    }

    uint32_t swapIndex = 0;
    vkCheck(
        vkAcquireNextImageKHR(
            dev_,
            swapchain_,
            UINT64_MAX,
            acquireSemaphore_,
            VK_NULL_HANDLE,
            &swapIndex),
        "vkAcquireNextImageKHR");

    VkCommandBuffer cmd = cmdBuffers_[swapIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer A");

    timestampsA_.reset(cmd, 0, QA_COUNT);
    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, QA_BEGIN_CONSUME);

    VkImage src = imports_[slot].image;
    VkImage dst = swapImages_[swapIndex];

    VkImageMemoryBarrier2 srcToTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = src,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageMemoryBarrier2 dstToTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = dst,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageMemoryBarrier2 beginBarriers[] = { srcToTransfer, dstToTransfer };

    VkDependencyInfo depBegin{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = beginBarriers
    };

    vkCmdPipelineBarrier2(cmd, &depBegin);

    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, QA_BEGIN_COMPOSE);

    VkImageBlit blit{
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = {
            { 0, 0, 0 },
            { static_cast<int32_t>(imageInfo_.width), static_cast<int32_t>(imageInfo_.height), 1 }
        },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = {
            { 0, 0, 0 },
            { static_cast<int32_t>(extent_.width), static_cast<int32_t>(extent_.height), 1 }
        }
    };

    vkCmdBlitImage(
        cmd,
        src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_LINEAR
    );

    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, QA_END_COMPOSE);

    VkImageMemoryBarrier2 srcBack{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = src,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageMemoryBarrier2 dstToPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = dst,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageMemoryBarrier2 endBarriers[] = { srcBack, dstToPresent };

    VkDependencyInfo depEnd{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = endBarriers
    };

    vkCmdPipelineBarrier2(cmd, &depEnd);

    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, QA_END_COMPUTE);

    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer A");

    VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = acquireSemaphore_,
        .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT
    };

    VkCommandBufferSubmitInfo cmdInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd
    };

    VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderCompleteSemaphore_,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    };

    VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo
    };

    vkCheck(vkQueueSubmit2(graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit2 A");

    VkPresentInfoKHR pi{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderCompleteSemaphore_,
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &swapIndex
    };

    vkCheck(vkQueuePresentKHR(presentQueue_, &pi), "vkQueuePresentKHR");
}