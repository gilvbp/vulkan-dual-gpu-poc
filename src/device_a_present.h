#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "shared_image.h"
#include "gpu_timestamps.h"

enum RenderQueriesA : uint32_t {
    QA_BEGIN_CONSUME = 0,
    QA_END_COMPUTE   = 1,
    QA_BEGIN_COMPOSE = 2,
    QA_END_COMPOSE   = 3,
    QA_COUNT         = 4
};

class DeviceAPresent {
public:
    void init(VkPhysicalDevice phys, VkDevice dev,
              uint32_t graphicsQueueFamily,
              uint32_t computeQueueFamily,
              VkQueue graphicsQueue,
              VkQueue computeQueue,
              VkQueue presentQueue);

    void createSharedTargets(uint32_t frameCount, const SharedImageCreateInfo& info);
    void uploadFrame(uint32_t slot, const void* data, VkDeviceSize size);
    void createSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height);

    void runComputePass(uint32_t slot, uint64_t timelineValue);
    void composeAndPresent(uint32_t slot);

    const GpuTimestamps& timestamps() const { return timestampsA_; }

private:
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice dev_ = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamily_ = 0;
    uint32_t computeQueueFamily_ = 0;

    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    SharedImageCreateInfo imageInfo_{};
    VkDeviceSize bufferSize_ = 0;

    std::vector<VkBuffer> uploadBuffers_;
    std::vector<VkDeviceMemory> uploadMemory_;
    std::vector<void*> mappedPtrs_;
    std::vector<VkImage> localImages_;
    std::vector<VkDeviceMemory> localImageMemory_;

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent_{};

    std::vector<VkImage> swapImages_;

    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmdBuffers_;

    VkSemaphore acquireSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderCompleteSemaphore_ = VK_NULL_HANDLE;

    GpuTimestamps timestampsA_;
};
