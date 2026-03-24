#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VkInstanceWrap {
public:
    void init(const std::vector<const char*>& extraExtensions = {});
    void destroy();
    VkInstance instance() const { return instance_; }
private:
    VkInstance instance_ = VK_NULL_HANDLE;
};
