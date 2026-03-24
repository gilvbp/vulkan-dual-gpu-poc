#include "device_a_present.h"
#include <iostream>

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

void DeviceAPresent::runComputePass(uint32_t slot, uint64_t timelineValue) {
    (void)slot;
    (void)timelineValue;
    // TODO: criar imagem local de saída na GPU A
    // TODO: descriptor sets + pipeline compute usando passthrough.comp
}

void DeviceAPresent::composeAndPresent(uint32_t slot) {
    std::cout << "compose/present TODO for slot " << slot << "\n";
    // TODO: integrar surface/swapchain real e pipeline gráfico fullscreen
    // TODO: bind da imported image como texture e present na GPU A
}
