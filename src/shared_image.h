#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

struct SharedImageCreateInfo {
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t width = 1920;
    uint32_t height = 1080;

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
};

struct ExportedImageHandle {
    int memoryFd = -1;
    VkDeviceSize allocationSize = 0;
    uint32_t memoryTypeIndex = 0;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct ImportedImageHandle {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};
