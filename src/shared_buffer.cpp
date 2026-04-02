#include "shared_buffer.h"

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

ExportedBufferHandle SharedBuffer::createExportable(
    VkPhysicalDevice phys,
    VkDevice device,
    const SharedBufferCreateInfo& ci)
{
    ExportedBufferHandle out{};

    VkExternalMemoryBufferCreateInfo extBuf{};
    extBuf.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    extBuf.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.pNext = &extBuf;
    bci.size = ci.size;
    bci.usage = ci.usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bci, nullptr, &out.buffer) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateBuffer export failed");
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, out.buffer, &req);

    out.allocationSize = req.size;
    out.memoryTypeIndex = findMemoryType(
        phys,
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkExportMemoryAllocateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &exportInfo;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = out.memoryTypeIndex;

    if (vkAllocateMemory(device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory export buffer failed");
    }

    if (vkBindBufferMemory(device, out.buffer, out.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindBufferMemory export failed");
    }

    auto vkGetMemoryFdKHR_ =
        reinterpret_cast<PFN_vkGetMemoryFdKHR>(
            vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHR_) {
        throw std::runtime_error("vkGetMemoryFdKHR unavailable");
    }

    VkMemoryGetFdInfoKHR fdInfo{};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = out.memory;
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    if (vkGetMemoryFdKHR_(device, &fdInfo, &out.memoryFd) != VK_SUCCESS) {
        throw std::runtime_error("vkGetMemoryFdKHR export buffer failed");
    }

    return out;
}

ImportedBufferHandle SharedBuffer::importFromFd(
    VkPhysicalDevice,
    VkDevice device,
    const SharedBufferCreateInfo& ci,
    int memoryFd,
    VkDeviceSize allocationSize,
    uint32_t exportedMemoryTypeIndex)
{
    ImportedBufferHandle out{};

    VkExternalMemoryBufferCreateInfo extBuf{};
    extBuf.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    extBuf.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.pNext = &extBuf;
    bci.size = ci.size;
    bci.usage = ci.usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bci, nullptr, &out.buffer) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateBuffer import failed");
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, out.buffer, &req);

    if ((req.memoryTypeBits & (1u << exportedMemoryTypeIndex)) == 0) {
        throw std::runtime_error("exported memoryTypeIndex incompatible with imported buffer");
    }

    VkImportMemoryFdInfoKHR importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    importInfo.fd = memoryFd;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &importInfo;
    mai.allocationSize = allocationSize;
    mai.memoryTypeIndex = exportedMemoryTypeIndex;

    if (vkAllocateMemory(device, &mai, nullptr, &out.memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory import buffer failed");
    }

    if (vkBindBufferMemory(device, out.buffer, out.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindBufferMemory import failed");
    }

    return out;
}
