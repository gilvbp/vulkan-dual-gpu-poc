#include "device_a_present.h"
#include "util.h"

#include <cstring>
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

    throw std::runtime_error("no local image memory type");
}

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

void DeviceAPresent::createSharedTargets(uint32_t frameCount, const SharedImageCreateInfo& info)
{
    imageInfo_ = info;
    bufferSize_ = static_cast<VkDeviceSize>(info.width) * info.height * 4;

    uploadBuffers_.resize(frameCount);
    uploadMemory_.resize(frameCount);
    mappedPtrs_.resize(frameCount, nullptr);
    localImages_.resize(frameCount);
    localImageMemory_.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = bufferSize_;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateBuffer(dev_, &bci, nullptr, &uploadBuffers_[i]), "vkCreateBuffer upload");

        VkMemoryRequirements reqBuf{};
        vkGetBufferMemoryRequirements(dev_, uploadBuffers_[i], &reqBuf);

        VkMemoryAllocateInfo maiBuf{};
        maiBuf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        maiBuf.allocationSize = reqBuf.size;
        maiBuf.memoryTypeIndex = findMemoryTypeLocal(
            phys_, reqBuf.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkCheck(vkAllocateMemory(dev_, &maiBuf, nullptr, &uploadMemory_[i]), "vkAllocateMemory upload");
        vkCheck(vkBindBufferMemory(dev_, uploadBuffers_[i], uploadMemory_[i], 0), "vkBindBufferMemory upload");
        vkCheck(vkMapMemory(dev_, uploadMemory_[i], 0, bufferSize_, 0, &mappedPtrs_[i]), "vkMapMemory upload");

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

        vkCheck(vkCreateImage(dev_, &ici, nullptr, &localImages_[i]), "vkCreateImage local image");

        VkMemoryRequirements reqImg{};
        vkGetImageMemoryRequirements(dev_, localImages_[i], &reqImg);

        VkMemoryAllocateInfo maiImg{};
        maiImg.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        maiImg.allocationSize = reqImg.size;
        maiImg.memoryTypeIndex = findMemoryTypeLocal(
            phys_, reqImg.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkCheck(vkAllocateMemory(dev_, &maiImg, nullptr, &localImageMemory_[i]), "vkAllocateMemory local image");
        vkCheck(vkBindImageMemory(dev_, localImages_[i], localImageMemory_[i], 0), "vkBindImageMemory local image");
    }
}

void DeviceAPresent::uploadFrame(uint32_t slot, const void* data, VkDeviceSize size)
{
    if (size > bufferSize_) throw std::runtime_error("uploadFrame size too large");
    std::memcpy(mappedPtrs_[slot], data, static_cast<size_t>(size));
}

void DeviceAPresent::createSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    surface_ = surface;
    extent_ = { width, height };

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface_;
    sci.minImageCount = 2;
    sci.imageFormat = swapFormat_;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = extent_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;

    vkCheck(vkCreateSwapchainKHR(dev_, &sci, nullptr, &swapchain_), "vkCreateSwapchainKHR");

    uint32_t count = 0;
    vkCheck(vkGetSwapchainImagesKHR(dev_, swapchain_, &count, nullptr), "vkGetSwapchainImagesKHR count");
    swapImages_.resize(count);
    vkCheck(vkGetSwapchainImagesKHR(dev_, swapchain_, &count, swapImages_.data()), "vkGetSwapchainImagesKHR data");

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = graphicsQueueFamily_;
    vkCheck(vkCreateCommandPool(dev_, &pci, nullptr, &cmdPool_), "vkCreateCommandPool A");

    cmdBuffers_.resize(std::max<size_t>(count, localImages_.size()));
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmdPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<uint32_t>(cmdBuffers_.size());
    vkCheck(vkAllocateCommandBuffers(dev_, &ai, cmdBuffers_.data()), "vkAllocateCommandBuffers A");

    VkSemaphoreCreateInfo sciSem{};
    sciSem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCheck(vkCreateSemaphore(dev_, &sciSem, nullptr, &acquireSemaphore_), "vkCreateSemaphore acquire");
    vkCheck(vkCreateSemaphore(dev_, &sciSem, nullptr, &renderCompleteSemaphore_), "vkCreateSemaphore renderComplete");
}

void DeviceAPresent::runComputePass(uint32_t slot, uint64_t) {
    VkCommandBuffer cmd = cmdBuffers_[slot];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer copy-to-image");

    timestampsA_.reset(cmd, 0, QA_COUNT);
    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, QA_BEGIN_CONSUME);

    VkBuffer srcBuffer = uploadBuffers_[slot];
    VkImage dstImage = localImages_[slot];

    VkImageMemoryBarrier2 toTransferDst{};
    toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransferDst.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    toTransferDst.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransferDst.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.image = dstImage;
    toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &toTransferDst;
    vkCmdPipelineBarrier2(cmd, &dep);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {imageInfo_.width, imageInfo_.height, 1};

    vkCmdCopyBufferToImage(cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier2 toTransferSrc{};
    toTransferSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransferSrc.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransferSrc.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransferSrc.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransferSrc.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    toTransferSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransferSrc.image = dstImage;
    toTransferSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dep2{};
    dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers = &toTransferSrc;
    vkCmdPipelineBarrier2(cmd, &dep2);

    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, QA_END_COMPUTE);

    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer copy-to-image");

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;

    vkCheck(vkQueueSubmit2(graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit2 copy-to-image");
    vkQueueWaitIdle(graphicsQueue_);
}

void DeviceAPresent::composeAndPresent(uint32_t slot) {
    uint32_t swapIndex = 0;
    vkCheck(vkAcquireNextImageKHR(dev_, swapchain_, UINT64_MAX, acquireSemaphore_, VK_NULL_HANDLE, &swapIndex), "vkAcquireNextImageKHR");

    VkCommandBuffer cmd = cmdBuffers_[swapIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCheck(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer A");

    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, QA_BEGIN_COMPOSE);

    VkImage src = localImages_[slot];
    VkImage dst = swapImages_[swapIndex];

    VkImageMemoryBarrier2 dstToTransfer{};
    dstToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    dstToTransfer.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    dstToTransfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    dstToTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    dstToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dstToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstToTransfer.image = dst;
    dstToTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo depBegin{};
    depBegin.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depBegin.imageMemoryBarrierCount = 1;
    depBegin.pImageMemoryBarriers = &dstToTransfer;
    vkCmdPipelineBarrier2(cmd, &depBegin);

    VkImageBlit blit{};
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.srcOffsets[1] = {static_cast<int32_t>(imageInfo_.width), static_cast<int32_t>(imageInfo_.height), 1};
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.dstOffsets[1] = {static_cast<int32_t>(extent_.width), static_cast<int32_t>(extent_.height), 1};

    vkCmdBlitImage(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    timestampsA_.write2(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, QA_END_COMPOSE);

    VkImageMemoryBarrier2 dstToPresent{};
    dstToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    dstToPresent.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    dstToPresent.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    dstToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    dstToPresent.image = dst;
    dstToPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo depEnd{};
    depEnd.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depEnd.imageMemoryBarrierCount = 1;
    depEnd.pImageMemoryBarriers = &dstToPresent;
    vkCmdPipelineBarrier2(cmd, &depEnd);

    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer A");

    VkSemaphoreSubmitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfo.semaphore = acquireSemaphore_;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = cmd;

    VkSemaphoreSubmitInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfo.semaphore = renderCompleteSemaphore_;
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &waitInfo;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signalInfo;

    vkCheck(vkQueueSubmit2(graphicsQueue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit2 A");

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderCompleteSemaphore_;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &swapIndex;

    vkCheck(vkQueuePresentKHR(presentQueue_, &pi), "vkQueuePresentKHR");
}
