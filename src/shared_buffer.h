#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

struct SharedBufferCreateInfo {
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
};

struct ExportedBufferHandle {
    int memoryFd = -1;
    VkDeviceSize allocationSize = 0;
    uint32_t memoryTypeIndex = 0;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct ImportedBufferHandle {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

class SharedBuffer {
public:
    static ExportedBufferHandle createExportable(
        VkPhysicalDevice phys,
        VkDevice device,
        const SharedBufferCreateInfo& ci);

    static ImportedBufferHandle importFromFd(
        VkPhysicalDevice phys,
        VkDevice device,
        const SharedBufferCreateInfo& ci,
        int memoryFd,
        VkDeviceSize allocationSize,
        uint32_t exportedMemoryTypeIndex);
};
