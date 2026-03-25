#include "device_b_render.h"
#include "util.h"

void DeviceBRender::init(VkPhysicalDevice phys, VkDevice dev, uint32_t graphicsQueueFamily, VkQueue queue) {
    phys_ = phys;
    dev_ = dev;
    queue_ = queue;
    queueFamily_ = graphicsQueueFamily;

    VkCommandPoolCreateInfo pci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamily_
    };

    vkCheck(vkCreateCommandPool(dev_, &pci, nullptr, &cmdPool_), "vkCreateCommandPool B");
    timestampsB_.init(phys_, dev_, QB_COUNT);
}

void DeviceBRender::createSharedTargets(uint32_t frameCount, const SharedImageCreateInfo& info) {
    imageInfo_ = info;
    exports_.clear();
    exports_.reserve(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        exports_.push_back(SharedImage::createExportable(phys_, dev_, info));
    }

    cmdBuffers_.resize(frameCount);

    VkCommandBufferAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdPool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(cmdBuffers_.size())
    };

    vkCheck(vkAllocateCommandBuffers(dev_, &ai, cmdBuffers_.data()), "vkAllocateCommandBuffers B");
}

void DeviceBRender::renderFrame(uint32_t slot, uint64_t frameId) {
    VkCommandBuffer cmd = cmdBuffers_[slot];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer B");

    timestampsB_.reset(cmd, 0, QB_COUNT);
    timestampsB_.write2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, QB_BEGIN_RENDER);

    VkImage image = exports_[slot].image;

    VkImageMemoryBarrier2 barrier1{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkDependencyInfo dep1{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier1
    };
    vkCmdPipelineBarrier2(cmd, &dep1);

    float t = static_cast<float>(frameId % 300) / 300.0f;
    VkClearColorValue color{{ t, 1.0f - t, 0.5f, 1.0f }};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdClearColorImage(
        cmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &color,
        1,
        &range
    );

    VkImageMemoryBarrier2 barrier2{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkDependencyInfo dep2{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier2
    };
    vkCmdPipelineBarrier2(cmd, &dep2);

    timestampsB_.write2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, QB_END_RENDER);

    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer B");

    VkCommandBufferSubmitInfo cmdInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd
    };

    VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdInfo
    };

    vkCheck(vkQueueSubmit2(queue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit2 B");
}

const ExportedImageHandle& DeviceBRender::exported(uint32_t slot) const {
    return exports_[slot];
}