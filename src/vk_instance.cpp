#include "vk_instance.h"
#include <stdexcept>

void VkInstanceWrap::init(const std::vector<const char*>& extraExtensions) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vulkan_dual_gpu_poc";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "none";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &appInfo;
    ici.enabledExtensionCount = static_cast<uint32_t>(extraExtensions.size());
    ici.ppEnabledExtensionNames = extraExtensions.empty() ? nullptr : extraExtensions.data();

    if (vkCreateInstance(&ici, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateInstance failed");
    }
}

void VkInstanceWrap::destroy() {
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}
