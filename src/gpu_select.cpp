#include "gpu_select.h"
#include <stdexcept>

std::vector<PhysicalGpuInfo> GpuSelect::enumerate(VkInstance instance) {
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        throw std::runtime_error("no Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devs(count);
    if (vkEnumeratePhysicalDevices(instance, &count, devs.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkEnumeratePhysicalDevices failed");
    }

    std::vector<PhysicalGpuInfo> out;
    out.reserve(count);

    for (auto phys : devs) {
        PhysicalGpuInfo info{};
        info.phys = phys;
        vkGetPhysicalDeviceProperties(phys, &info.props);
        vkGetPhysicalDeviceMemoryProperties(phys, &info.memProps);

        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &qCount, qProps.data());

        for (uint32_t i = 0; i < qCount; ++i) {
            if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && info.qf.graphics == UINT32_MAX) {
                info.qf.graphics = i;
            }
            if ((qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && info.qf.compute == UINT32_MAX) {
                info.qf.compute = i;
            }
        }
        if (info.qf.compute == UINT32_MAX) info.qf.compute = info.qf.graphics;
        out.push_back(info);
    }

    return out;
}

int GpuSelect::findDiscreteGpu(const std::vector<PhysicalGpuInfo>& gpus, int skipIndex) {
    for (int i = 0; i < static_cast<int>(gpus.size()); ++i) {
        if (i == skipIndex) continue;
        if (gpus[i].props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            return i;
        }
    }
    return -1;
}
