#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "shared_image.h"

class DeviceBRender {
public:
    void init(VkPhysicalDevice phys, VkDevice dev, uint32_t graphicsQueueFamily, VkQueue queue);
    void createSharedTargets(uint32_t frameCount, const SharedImageCreateInfo& info);
    void renderFrame(uint32_t slot, uint64_t frameId);
    const ExportedImageHandle& exported(uint32_t slot) const;

private:
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice dev_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    SharedImageCreateInfo imageInfo_{};
    std::vector<ExportedImageHandle> exports_;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmdBuffers_;
};
