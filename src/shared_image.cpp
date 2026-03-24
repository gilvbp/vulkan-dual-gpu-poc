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
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) {
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

    VkExternalMemoryImageCreateInfo extImg{};
    extImg.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extImg.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.pNext = &extImg;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = ci.format;
    ici.extent = {ci.width, ci.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = ci.usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &ici, nullptr, &out.image) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage failed");
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, out.image, &req);
    out.allocationSize = req.size;

    VkExportMemoryAllocateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &exportInfo;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory failed");
    }
    if (vkBindImageMemory(device, out.image, out.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindImageMemory failed");
    }

    auto vkGetMemoryFdKHR_ = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHR_) throw std::runtime_error("vkGetMemoryFdKHR unavailable");

    VkMemoryGetFdInfoKHR fdInfo{};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = out.memory;
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

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

    VkExternalMemoryImageCreateInfo extImg{};
    extImg.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extImg.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.pNext = &extImg;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = ci.format;
    ici.extent = {ci.width, ci.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = ci.usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &ici, nullptr, &out.image) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage import failed");
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, out.image, &req);

    VkImportMemoryFdInfoKHR importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    importInfo.fd = memoryFd;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &importInfo;
    mai.allocationSize = allocationSize;
    mai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory import failed");
    }
    if (vkBindImageMemory(device, out.image, out.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindImageMemory import failed");
    }

    return out;
}
