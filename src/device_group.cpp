#include "device_group.h"
#include <stdexcept>

std::vector<DeviceGroupInfo> DeviceGroup::enumerate(VkInstance instance) {
    uint32_t count = 0;
    VkResult r = vkEnumeratePhysicalDeviceGroups(instance, &count, nullptr);
    if (r != VK_SUCCESS || count == 0) {
        throw std::runtime_error("vkEnumeratePhysicalDeviceGroups count failed");
    }

    std::vector<VkPhysicalDeviceGroupProperties> props(count);
    for (auto& p : props) {
        p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
    }

    r = vkEnumeratePhysicalDeviceGroups(instance, &count, props.data());
    if (r != VK_SUCCESS) {
        throw std::runtime_error("vkEnumeratePhysicalDeviceGroups data failed");
    }

    std::vector<DeviceGroupInfo> out;
    out.reserve(count);

    for (const auto& g : props) {
        DeviceGroupInfo info{};
        info.subsetAllocation = (g.subsetAllocation == VK_TRUE);
        info.physicalDevices.assign(g.physicalDevices, g.physicalDevices + g.physicalDeviceCount);
        out.push_back(std::move(info));
    }

    return out;
}
