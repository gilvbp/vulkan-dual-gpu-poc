#include "shared_image.h"

#include <stdexcept>

static uint32_t findMemoryType(
    VkPhysicalDevice phys,
    uint32_t typeBits,
    VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }

    throw std::runtime_error("no suitable memory type");
}

ExportedImageHandle SharedImage::createExportable(
    VkPhysicalDevice phys,
    VkDevice device,
    const SharedImageCreateInfo& ci)
{
    ExportedImageHandle out{};

    VkExternalMemoryImageCreateInfo extImg{
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
    };

    VkImageCreateInfo ici{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &extImg,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ci.format,
        .extent = {ci.width, ci.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = ci.usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (vkCreateImage(device, &ici, nullptr, &out.image) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage failed");
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, out.image, &req);
    out.allocationSize = req.size;

    VkExportMemoryAllocateInfo exportInfo{
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
    };

    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &exportInfo,
        .allocationSize = req.size,
        .memoryTypeIndex = findMemoryType(
            phys,
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    if (vkAllocateMemory(device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory failed");
    }

    if (vkBindImageMemory(device, out.image, out.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindImageMemory failed");
    }

    auto vkGetMemoryFdKHR_ =
        reinterpret_cast<PFN_vkGetMemoryFdKHR>(
            vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHR_) {
        throw std::runtime_error("vkGetMemoryFdKHR unavailable");
    }

    VkMemoryGetFdInfoKHR fdInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = out.memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
    };

    if (vkGetMemoryFdKHR_(device, &fdInfo, &out.memoryFd) != VK_SUCCESS) {
        throw std::runtime_error("vkGetMemoryFdKHR failed");
    }

    return out;
}

ImportedImageHandle SharedImage::importFromFd(
    VkPhysicalDevice phys,
    VkDevice device,
    const SharedImageCreateInfo& ci,
    int memoryFd,
    VkDeviceSize allocationSize)
{
    ImportedImageHandle out{};

    VkExternalMemoryImageCreateInfo extImg{
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
    };

    VkImageCreateInfo ici{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &extImg,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ci.format,
        .extent = {ci.width, ci.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = ci.usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (vkCreateImage(device, &ici, nullptr, &out.image) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage import failed");
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, out.image, &req);

    auto vkGetMemoryFdPropertiesKHR_ =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR"));
    if (!vkGetMemoryFdPropertiesKHR_) {
        throw std::runtime_error("vkGetMemoryFdPropertiesKHR unavailable");
    }

    VkMemoryFdPropertiesKHR fdProps{
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR
    };

    if (vkGetMemoryFdPropertiesKHR_(
            device,
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
            memoryFd,
            &fdProps) != VK_SUCCESS) {
        throw std::runtime_error("vkGetMemoryFdPropertiesKHR failed");
    }

    uint32_t compatibleBits = req.memoryTypeBits & fdProps.memoryTypeBits;
    if (compatibleBits == 0) {
        throw std::runtime_error("no compatible memory type bits for imported fd");
    }

    VkImportMemoryFdInfoKHR importInfo{
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
        .fd = memoryFd
    };

    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &importInfo,
        .allocationSize = allocationSize,
        .memoryTypeIndex = findMemoryType(
            phys,
            compatibleBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    if (vkAllocateMemory(device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory import failed");
    }

    if (vkBindImageMemory(device, out.image, out.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindImageMemory import failed");
    }

    return out;
}