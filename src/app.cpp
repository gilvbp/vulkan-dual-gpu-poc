#include "app.h"
#include "util.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

static VkDevice createLogicalDevice(
    VkPhysicalDevice phys,
    const QueueFamilySelection& qf,
    const std::vector<const char*>& exts)
{
    std::set<uint32_t> uniqueQueues = { qf.graphics, qf.compute };

    float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    qcis.reserve(uniqueQueues.size());

    for (uint32_t q : uniqueQueues) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = q;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{};
    timelineFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timelineFeature.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.pNext = &timelineFeature;
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
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(1280, 720, "vulkan-dual-gpu-poc", nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("glfwCreateWindow failed");
    }

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> instanceExts(glfwExts, glfwExts + glfwExtCount);

    instanceWrap_.init(instanceExts);

    if (glfwCreateWindowSurface(instanceWrap_.instance(), window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("glfwCreateWindowSurface failed");
    }

    gpus_ = GpuSelect::enumerate(instanceWrap_.instance());
    if (gpus_.size() < 2) {
        throw std::runtime_error("need at least 2 Vulkan GPUs");
    }

    gpuAIndex_ = 0;
    gpuBIndex_ = 1;

    std::cout << "GPU A: " << gpus_[gpuAIndex_].props.deviceName << "\n";
    std::cout << "GPU B: " << gpus_[gpuBIndex_].props.deviceName << "\n";

    createDevices();
    createSharedResources();

    presentA_.createSwapchain(surface_, 1280, 720);
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

    deviceA_ = createLogicalDevice(gpus_[gpuAIndex_].phys, gpus_[gpuAIndex_].qf, extsA);
    deviceB_ = createLogicalDevice(gpus_[gpuBIndex_].phys, gpus_[gpuBIndex_].qf, extsB);

    vkGetDeviceQueue(deviceA_, gpus_[gpuAIndex_].qf.graphics, 0, &graphicsQueueA_);
    vkGetDeviceQueue(deviceA_, gpus_[gpuAIndex_].qf.compute, 0, &computeQueueA_);
    presentQueueA_ = graphicsQueueA_;

    vkGetDeviceQueue(deviceB_, gpus_[gpuBIndex_].qf.graphics, 0, &graphicsQueueB_);

    renderB_.init(
        gpus_[gpuBIndex_].phys,
        deviceB_,
        gpus_[gpuBIndex_].qf.graphics,
        graphicsQueueB_);

    presentA_.init(
        gpus_[gpuAIndex_].phys,
        deviceA_,
        gpus_[gpuAIndex_].qf.graphics,
        gpus_[gpuAIndex_].qf.compute,
        graphicsQueueA_,
        computeQueueA_,
        presentQueueA_);
}

void App::createSharedResources() {
    sharedInfo_.width = 1920;
    sharedInfo_.height = 1080;
    sharedInfo_.format = VK_FORMAT_R8G8B8A8_UNORM;

    renderB_.createSharedTargets(frameSlots_, sharedInfo_);

    std::vector<ExportedBufferHandle> exported;
    exported.reserve(frameSlots_);
    for (uint32_t i = 0; i < frameSlots_; ++i) {
        exported.push_back(renderB_.exportedBuffer(i));
    }

    presentA_.importSharedTargets(frameSlots_, sharedInfo_, exported);

    renderTimelineExport_ = TimelineSync::createExportableTimeline(deviceB_, 0);
    renderTimelineOnA_ = TimelineSync::importTimelineFromFd(deviceA_, renderTimelineExport_.fd);
}

void App::loop() {
    uint64_t frameId = 1;

    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        uint32_t renderSlot = static_cast<uint32_t>((frameId - 1) % frameSlots_);
        renderB_.renderFrame(renderSlot, frameId);
        mailbox_.publishRendered(frameId, renderSlot);

        uint64_t chosenFrameId = 0;
        uint32_t chosenSlot = 0;

        if (lowLatencyMode_) {
            if (!mailbox_.tryAcquireLatest(chosenFrameId, chosenSlot)) {
                ++frameId;
                continue;
            }
        } else {
            chosenFrameId = frameId;
            chosenSlot = renderSlot;
        }

        auto cpuBeforeWait = std::chrono::steady_clock::now();

        TimelineSync::hostSignal(deviceB_, renderTimelineExport_.semaphore, chosenFrameId);
        TimelineSync::hostWait(deviceA_, renderTimelineOnA_, chosenFrameId, 1'000'000'000ull);

        auto cpuAfterWait = std::chrono::steady_clock::now();

        double waitMs =
            std::chrono::duration<double, std::milli>(cpuAfterWait - cpuBeforeWait).count();

        presentA_.runComputePass(chosenSlot, chosenFrameId);
        presentA_.composeAndPresent(chosenSlot);

        mailbox_.markConsumed(chosenFrameId);

        vkDeviceWaitIdle(deviceB_);

        auto qb0 = renderB_.timestamps().readOne(QB_BEGIN_RENDER);
        auto qb1 = renderB_.timestamps().readOne(QB_END_RENDER);

        if (qb0.available && qb1.available) {
            uint64_t dt = qb1.value - qb0.value;
            double renderMs = renderB_.timestamps().ticksToMilliseconds(dt);

            std::cout
                << "[frame " << chosenFrameId << "] "
                << "GPU_B_render=" << renderMs << " ms, "
                << "CPU_wait=" << waitMs << " ms\n";
        }

        ++frameId;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    vkDeviceWaitIdle(deviceA_);
    vkDeviceWaitIdle(deviceB_);
}

void App::shutdown() {
    if (renderTimelineOnA_) {
        vkDestroySemaphore(deviceA_, renderTimelineOnA_, nullptr);
        renderTimelineOnA_ = VK_NULL_HANDLE;
    }

    if (renderTimelineExport_.semaphore) {
        vkDestroySemaphore(deviceB_, renderTimelineExport_.semaphore, nullptr);
        renderTimelineExport_.semaphore = VK_NULL_HANDLE;
    }

    if (deviceA_) {
        vkDestroyDevice(deviceA_, nullptr);
        deviceA_ = VK_NULL_HANDLE;
    }

    if (deviceB_) {
        vkDestroyDevice(deviceB_, nullptr);
        deviceB_ = VK_NULL_HANDLE;
    }

    if (surface_) {
        vkDestroySurfaceKHR(instanceWrap_.instance(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
    instanceWrap_.destroy();
}
