#include "app.h"
#include "util.h"
#include <iostream>
#include <set>
#include <thread>
#include <chrono>
#include <stdexcept>

static VkDevice createLogicalDevice13(
    VkPhysicalDevice phys,
    const QueueFamilySelection& qf,
    const std::vector<const char*>& exts)
{
    std::set<uint32_t> uniqueQueues = { qf.graphics, qf.compute };

    float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (uint32_t family : uniqueQueues) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceTimelineSemaphoreFeatures timeline{};
    timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timeline.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.pNext = &timeline;
    dynamicRendering.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.pNext = &dynamicRendering;
    sync2.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &sync2;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &features2;
    dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos = qcis.data();
    dci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    dci.ppEnabledExtensionNames = exts.data();

    VkDevice dev = VK_NULL_HANDLE;
    vkCheck(vkCreateDevice(phys, &dci, nullptr, &dev), "vkCreateDevice");
    return dev;
}

void App::run() {
    init();
    loop();
    shutdown();
}

void App::init() {
    instanceWrap_.init({});
    gpus_ = GpuSelect::enumerate(instanceWrap_.instance());
    if (gpus_.size() < 2) {
        throw std::runtime_error("need at least 2 Vulkan GPUs for this PoC");
    }

    gpuAIndex_ = 0;
    gpuBIndex_ = 1;

    std::cout << "GPU A: " << gpus_[gpuAIndex_].props.deviceName << "\n";
    std::cout << "GPU B: " << gpus_[gpuBIndex_].props.deviceName << "\n";

    createDevices();
    createSharedResources();
}

void App::createDevices() {
    std::vector<const char*> extsA = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
    };

    std::vector<const char*> extsB = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
    };

    deviceA_ = createLogicalDevice13(gpus_[gpuAIndex_].phys, gpus_[gpuAIndex_].qf, extsA);
    deviceB_ = createLogicalDevice13(gpus_[gpuBIndex_].phys, gpus_[gpuBIndex_].qf, extsB);

    vkGetDeviceQueue(deviceA_, gpus_[gpuAIndex_].qf.graphics, 0, &graphicsQueueA_);
    vkGetDeviceQueue(deviceA_, gpus_[gpuAIndex_].qf.compute, 0, &computeQueueA_);
    presentQueueA_ = graphicsQueueA_;
    vkGetDeviceQueue(deviceB_, gpus_[gpuBIndex_].qf.graphics, 0, &graphicsQueueB_);

    renderB_.init(gpus_[gpuBIndex_].phys, deviceB_, gpus_[gpuBIndex_].qf.graphics, graphicsQueueB_);
    presentA_.init(gpus_[gpuAIndex_].phys, deviceA_, gpus_[gpuAIndex_].qf.graphics,
                   gpus_[gpuAIndex_].qf.compute, graphicsQueueA_, computeQueueA_, presentQueueA_);
}

void App::createSharedResources() {
    sharedInfo_.width = 1920;
    sharedInfo_.height = 1080;
    sharedInfo_.format = VK_FORMAT_R8G8B8A8_UNORM;

    renderB_.createSharedTargets(frameSlots_, sharedInfo_);

    std::vector<ExportedImageHandle> exported;
    exported.reserve(frameSlots_);
    for (uint32_t i = 0; i < frameSlots_; ++i) {
        exported.push_back(renderB_.exported(i));
    }
    presentA_.importSharedTargets(frameSlots_, sharedInfo_, exported);

    renderTimelineExport_ = TimelineSync::createExportableTimeline(deviceB_, 0);
    renderTimelineOnA_ = TimelineSync::importTimelineFromFd(deviceA_, renderTimelineExport_.fd);
}

void App::loop() {
    uint64_t frameId = 1;
    while (running_ && frameId <= 60) {
        uint32_t slot = static_cast<uint32_t>((frameId - 1) % frameSlots_);
        renderB_.renderFrame(slot, frameId);
        TimelineSync::hostSignal(deviceB_, renderTimelineExport_.semaphore, frameId);
        TimelineSync::hostWait(deviceA_, renderTimelineOnA_, frameId, 1000000000ull);
        presentA_.runComputePass(slot, frameId);
        presentA_.composeAndPresent(slot);
        std::cout << "frame " << frameId << " slot " << slot << "\n";
        ++frameId;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    vkDeviceWaitIdle(deviceA_);
    vkDeviceWaitIdle(deviceB_);
}

void App::shutdown() {
    if (renderTimelineOnA_) vkDestroySemaphore(deviceA_, renderTimelineOnA_, nullptr);
    if (renderTimelineExport_.semaphore) vkDestroySemaphore(deviceB_, renderTimelineExport_.semaphore, nullptr);
    if (deviceA_) vkDestroyDevice(deviceA_, nullptr);
    if (deviceB_) vkDestroyDevice(deviceB_, nullptr);
    instanceWrap_.destroy();
}
