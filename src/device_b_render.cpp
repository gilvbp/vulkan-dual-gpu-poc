#include "device_b_render.h"
#include "util.h"

#include <stdexcept>

static uint32_t findMemoryTypeLocal(
    VkPhysicalDevice phys,
    uint32_t typeBits,
    VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }

    throw std::runtime_error("no suitable memory type");
}

void DeviceBRender::init(
    VkPhysicalDevice phys,
    VkDevice dev,
    uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue)
{
    phys_ = phys;
    dev_ = dev;
    queueFamily_ = graphicsQueueFamily;
    queue_ = graphicsQueue;

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = queueFamily_;

    vkCheck(vkCreateCommandPool(dev_, &pci, nullptr, &cmdPool_), "vkCreateCommandPool B");
    timestampsB_.init(phys_, dev_, QB_COUNT);
}

void DeviceBRender::createSharedTargets(
    uint32_t frameCount,
    const SharedImageCreateInfo& info)
{
    imageInfo_ = info;
    bufferInfo_.size = static_cast<VkDeviceSize>(info.width) * info.height * 4;

    renderImages_.resize(frameCount);
    renderImageMemory_.resize(frameCount);
    exports_.clear();
    exports_.reserve(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        VkImageCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = info.format;
        ici.extent = {info.width, info.height, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkCheck(vkCreateImage(dev_, &ici, nullptr, &renderImages_[i]), "vkCreateImage render image");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(dev_, renderImages_[i], &req);

        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = findMemoryTypeLocal(
            phys_,
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkCheck(vkAllocateMemory(dev_, &mai, nullptr, &renderImageMemory_[i]), "vkAllocateMemory render image");
        vkCheck(vkBindImageMemory(dev_, renderImages_[i], renderImageMemory_[i], 0), "vkBindImageMemory render image");

        exports_.push_back(SharedBuffer::createExportable(phys_, dev_, bufferInfo_));
    }

    cmdBuffers_.resize(frameCount);

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmdPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<uint32_t>(cmdBuffers_.size());

    vkCheck(vkAllocateCommandBuffers(dev_, &ai, cmdBuffers_.data()), "vkAllocateCommandBuffers B");
}

void DeviceBRender::renderFrame(uint32_t slot, uint64_t frameId)
{
    VkCommandBuffer cmd = cmdBuffers_[slot];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer B");

    timestampsB_.reset(cmd, 0, QB_COUNT);
    timestampsB_.write2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, QB_BEGIN_RENDER);

    VkImage image = renderImages_[slot];
    VkBuffer buffer = exports_[slot].buffer;

    VkImageMemoryBarrier2 toTransferDst{};
    toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransferDst.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    toTransferDst.srcAccessMask = 0;
    toTransferDst.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransferDst.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.image = image;
    toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep0{};
    dep0.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep0.imageMemoryBarrierCount = 1;
    dep0.pImageMemoryBarriers = &toTransferDst;
    vkCmdPipelineBarrier2(cmd, &dep0);

    float t = (frameId % 255) / 255.0f;
    VkClearColorValue clearColor{{t, 0.2f, 1.0f - t, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdClearColorImage(
        cmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clearColor,
        1,
        &range);

    VkImageMemoryBarrier2 toTransferSrc{};
    toTransferSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransferSrc.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransferSrc.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransferSrc.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransferSrc.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    toTransferSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransferSrc.image = image;
    toTransferSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep1{};
    dep1.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers = &toTransferSrc;
    vkCmdPipelineBarrier2(cmd, &dep1);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {imageInfo_.width, imageInfo_.height, 1};

    vkCmdCopyImageToBuffer(
        cmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        buffer,
        1,
        &region);

    timestampsB_.write2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, QB_END_RENDER);

    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer B");

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;

    vkCheck(vkQueueSubmit2(queue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit2 B");
}
