#pragma once
#include "vk_instance.h"
#include "gpu_select.h"
#include "device_b_render.h"
#include "device_a_present.h"
#include "frame_mailbox.h"
#include <vector>
#include <GLFW/glfw3.h>

class App {
public:
    void run();

private:
    void init();
    void loop();
    void shutdown();
    void createDevices();
    void createSharedResources();

private:
    VkInstanceWrap instanceWrap_;
    std::vector<PhysicalGpuInfo> gpus_;

    int gpuAIndex_ = -1;
    int gpuBIndex_ = -1;

    VkDevice deviceA_ = VK_NULL_HANDLE;
    VkDevice deviceB_ = VK_NULL_HANDLE;

    VkQueue graphicsQueueA_ = VK_NULL_HANDLE;
    VkQueue computeQueueA_ = VK_NULL_HANDLE;
    VkQueue presentQueueA_ = VK_NULL_HANDLE;
    VkQueue graphicsQueueB_ = VK_NULL_HANDLE;

    DeviceBRender renderB_;
    DeviceAPresent presentA_;

    SharedImageCreateInfo sharedInfo_{};
    uint32_t frameSlots_ = 2;

    FrameMailbox mailbox_;
    bool lowLatencyMode_ = true;
    bool running_ = true;

    GLFWwindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};
