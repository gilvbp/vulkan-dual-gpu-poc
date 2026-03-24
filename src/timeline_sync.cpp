#include "timeline_sync.h"
#include <stdexcept>

VkSemaphore TimelineSync::createTimeline(VkDevice device, uint64_t initialValue) {
    VkSemaphoreTypeCreateInfo typeInfo{};
    typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeInfo.initialValue = initialValue;

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sci.pNext = &typeInfo;

    VkSemaphore sem = VK_NULL_HANDLE;
    if (vkCreateSemaphore(device, &sci, nullptr, &sem) != VK_SUCCESS) {
        throw std::runtime_error("create timeline semaphore failed");
    }
    return sem;
}

ExportedTimelineSemaphore TimelineSync::createExportableTimeline(VkDevice device, uint64_t initialValue) {
    ExportedTimelineSemaphore out{};

    VkExportSemaphoreCreateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkSemaphoreTypeCreateInfo typeInfo{};
    typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    typeInfo.pNext = &exportInfo;
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeInfo.initialValue = initialValue;

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sci.pNext = &typeInfo;

    if (vkCreateSemaphore(device, &sci, nullptr, &out.semaphore) != VK_SUCCESS) {
        throw std::runtime_error("create exportable timeline failed");
    }

    auto vkGetSemaphoreFdKHR_ = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));
    if (!vkGetSemaphoreFdKHR_) throw std::runtime_error("vkGetSemaphoreFdKHR unavailable");

    VkSemaphoreGetFdInfoKHR fdInfo{};
    fdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    fdInfo.semaphore = out.semaphore;
    fdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    if (vkGetSemaphoreFdKHR_(device, &fdInfo, &out.fd) != VK_SUCCESS) {
        throw std::runtime_error("vkGetSemaphoreFdKHR failed");
    }

    return out;
}

VkSemaphore TimelineSync::importTimelineFromFd(VkDevice device, int fd) {
    VkSemaphore sem = createTimeline(device, 0);
    auto vkImportSemaphoreFdKHR_ = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkImportSemaphoreFdKHR"));
    if (!vkImportSemaphoreFdKHR_) throw std::runtime_error("vkImportSemaphoreFdKHR unavailable");

    VkImportSemaphoreFdInfoKHR importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importInfo.semaphore = sem;
    importInfo.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
    importInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    importInfo.fd = fd;

    if (vkImportSemaphoreFdKHR_(device, &importInfo) != VK_SUCCESS) {
        throw std::runtime_error("vkImportSemaphoreFdKHR failed");
    }
    return sem;
}

void TimelineSync::hostSignal(VkDevice device, VkSemaphore semaphore, uint64_t value) {
    VkSemaphoreSignalInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    info.semaphore = semaphore;
    info.value = value;
    if (vkSignalSemaphore(device, &info) != VK_SUCCESS) {
        throw std::runtime_error("vkSignalSemaphore failed");
    }
}

void TimelineSync::hostWait(VkDevice device, VkSemaphore semaphore, uint64_t value, uint64_t timeoutNs) {
    VkSemaphoreWaitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    info.semaphoreCount = 1;
    info.pSemaphores = &semaphore;
    info.pValues = &value;
    if (vkWaitSemaphores(device, &info, timeoutNs) != VK_SUCCESS) {
        throw std::runtime_error("vkWaitSemaphores failed");
    }
}
