#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "shared_image.h"

class DeviceAPresent {
public:
    void init(VkPhysicalDevice phys, VkDevice dev,
              uint32_t graphicsQueueFamily,
              uint32_t computeQueueFamily,
              VkQueue graphicsQueue,
              VkQueue computeQueue,
              VkQueue presentQueue);

    void importSharedTargets(uint32_t frameCount,
                             const SharedImageCreateInfo& info,
                             const std::vector<ExportedImageHandle>& exported);

    void runComputePass(uint32_t slot, uint64_t timelineValue);
    void composeAndPresent(uint32_t slot);

private:
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice dev_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    uint32_t computeQueueFamily_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    SharedImageCreateInfo imageInfo_{};
    std::vector<ImportedImageHandle> imports_;
};
