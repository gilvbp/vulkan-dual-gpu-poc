#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

inline void vkCheck(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string(msg) + " failed with VkResult=" + std::to_string(r));
    }
}
