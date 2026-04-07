#pragma once
#include <vulkan/vulkan.h>
#include <vector>

struct DeviceGroupInfo {
    std::vector<VkPhysicalDevice> physicalDevices;
    bool subsetAllocation = false;
};

class DeviceGroup {
public:
    static std::vector<DeviceGroupInfo> enumerate(VkInstance instance);
};
