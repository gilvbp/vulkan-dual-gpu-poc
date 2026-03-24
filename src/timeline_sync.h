#pragma once
#include <vulkan/vulkan.h>

struct ExportedTimelineSemaphore {
    VkSemaphore semaphore = VK_NULL_HANDLE;
    int fd = -1;
};

class TimelineSync {
public:
    static VkSemaphore createTimeline(VkDevice device, uint64_t initialValue = 0);
    static ExportedTimelineSemaphore createExportableTimeline(VkDevice device, uint64_t initialValue = 0);
    static VkSemaphore importTimelineFromFd(VkDevice device, int fd);
    static void hostSignal(VkDevice device, VkSemaphore semaphore, uint64_t value);
    static void hostWait(VkDevice device, VkSemaphore semaphore, uint64_t value, uint64_t timeoutNs);
};
