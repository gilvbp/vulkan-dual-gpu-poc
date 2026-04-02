#pragma once
#include <vulkan/vulkan.h>
#include <vector>

struct TimestampReadback {
    uint64_t value = 0;
    bool available = false;
};

class GpuTimestamps {
public:
    void init(VkPhysicalDevice phys, VkDevice dev, uint32_t queryCount);
    void destroy();

    void reset(VkCommandBuffer cmd, uint32_t firstQuery, uint32_t queryCount);
    void write2(VkCommandBuffer cmd, VkPipelineStageFlags2 stage, uint32_t queryIndex);

    TimestampReadback readOne(uint32_t queryIndex) const;

    double ticksToNanoseconds(uint64_t ticks) const;
    double ticksToMilliseconds(uint64_t ticks) const;

private:
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice dev_ = VK_NULL_HANDLE;
    VkQueryPool pool_ = VK_NULL_HANDLE;
    uint32_t queryCount_ = 0;
    float timestampPeriod_ = 1.0f;
};
