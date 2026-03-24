#include "device_b_render.h"
#include "shared_image.h"
#include "util.h"

void DeviceBRender::init(VkPhysicalDevice phys, VkDevice dev, uint32_t graphicsQueueFamily, VkQueue queue) {
    phys_ = phys;
    dev_ = dev;
    queue_ = queue;
    queueFamily_ = graphicsQueueFamily;

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = queueFamily_;
    vkCheck(vkCreateCommandPool(dev_, &pci, nullptr, &cmdPool_), "create cmd pool");
}

void DeviceBRender::createSharedTargets(uint32_t frameCount, const SharedImageCreateInfo& info) {
    imageInfo_ = info;
    exports_.clear();
    exports_.reserve(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        exports_.push_back(SharedImage::createExportable(phys_, dev_, info));
    }

    cmdBuffers_.resize(frameCount);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmdPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = frameCount;
    vkCheck(vkAllocateCommandBuffers(dev_, &ai, cmdBuffers_.data()), "alloc cmd buffers");
}

void DeviceBRender::renderFrame(uint32_t slot, uint64_t frameId) {
    VkCommandBuffer cmd = cmdBuffers_[slot];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    VkImage image = exports_[slot].image;

    VkImageMemoryBarrier2 barrier1{};
    barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier1.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier1.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier1.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier1.image = image;
    barrier1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep1{};
    dep1.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers = &barrier1;
    vkCmdPipelineBarrier2(cmd, &dep1);

    float t = static_cast<float>(frameId % 300) / 300.0f;
    VkClearColorValue color{};
    color.float32[0] = t;
    color.float32[1] = 1.0f - t;
    color.float32[2] = 0.5f;
    color.float32[3] = 1.0f;

    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

    VkImageMemoryBarrier2 barrier2{};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.image = image;
    barrier2.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep2{};
    dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers = &barrier2;
    vkCmdPipelineBarrier2(cmd, &dep2);

    vkEndCommandBuffer(cmd);

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;

    vkQueueSubmit2(queue_, 1, &submit, VK_NULL_HANDLE);
}

const ExportedImageHandle& DeviceBRender::exported(uint32_t slot) const {
    return exports_[slot];
}
