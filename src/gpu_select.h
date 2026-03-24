#pragma once
#include <vulkan/vulkan.h>
#include <vector>

struct QueueFamilySelection {
    uint32_t graphics = UINT32_MAX;
    uint32_t compute = UINT32_MAX;
    uint32_t present = UINT32_MAX;
};

struct PhysicalGpuInfo {
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceMemoryProperties memProps{};
    QueueFamilySelection qf{};
};

class GpuSelect {
public:
    static std::vector<PhysicalGpuInfo> enumerate(VkInstance instance);
    static int findDiscreteGpu(const std::vector<PhysicalGpuInfo>& gpus, int skipIndex = -1);
};
