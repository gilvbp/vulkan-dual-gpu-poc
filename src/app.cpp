#include "app.h"
#include "util.h"
#include <iostream>
#include <set>
#include <thread>
#include <chrono>

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
        VkDeviceQueueCreateInfo qci{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = q,
            .queueCount = 1,
            .pQueuePriorities = &prio
        };
        qcis.push_back(qci);
    }

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .timelineSemaphore = VK_TRUE
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &timelineFeature,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceSynchronization2Features sync2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &dynamicRendering,
        .synchronization2 = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &sync2
    };

    VkDeviceCreateInfo dci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = static_cast<uint32_t>(qcis.size()),
        .pQueueCreateInfos = qcis.data(),
        .enabledExtensionCount = static_cast<uint32_t>(exts.size()),
        .ppEnabledExtensionNames = exts.data()
    };

    VkDevice dev = VK_NULL_HANDLE;
    vkCheck(vkCreateDevice(phys, &dci, nullptr, &dev), "vkCreateDevice");
    return dev;
}

// ... resto do init igual ao teu scaffold ...

void App::loop() {
    uint64_t frameId = 1;

    while (running_ && frameId <= 300) {
        uint32_t slot = static_cast<uint32_t>((frameId - 1) % frameSlots_);

        renderB_.renderFrame(slot, frameId);

        auto cpuBeforeWait = std::chrono::steady_clock::now();
        TimelineSync::hostSignal(deviceB_, renderTimelineExport_.semaphore, frameId);
        TimelineSync::hostWait(deviceA_, renderTimelineOnA_, frameId, 1'000'000'000ull);
        auto cpuAfterWait = std::chrono::steady_clock::now();

        double waitMs =
            std::chrono::duration<double, std::milli>(cpuAfterWait - cpuBeforeWait).count();

        presentA_.runComputePass(slot, frameId);
        presentA_.composeAndPresent(slot);

        // Dê um tempinho para queries ficarem disponíveis no começo
        vkDeviceWaitIdle(deviceB_);

        auto qb0 = renderB_.timestamps().readOne(QB_BEGIN_RENDER);
        auto qb1 = renderB_.timestamps().readOne(QB_END_RENDER);

        if (qb0.available && qb1.available) {
            uint64_t dt = qb1.value - qb0.value;
            double renderMs = renderB_.timestamps().ticksToMilliseconds(dt);

            std::cout
                << "[frame " << frameId << "] "
                << "GPU_B_render=" << renderMs << " ms, "
                << "CPU_wait_B_to_A=" << waitMs << " ms"
                << "\n";
        } else {
            std::cout
                << "[frame " << frameId << "] "
                << "timestamps not ready, "
                << "CPU_wait_B_to_A=" << waitMs << " ms"
                << "\n";
        }

        ++frameId;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    vkDeviceWaitIdle(deviceA_);
    vkDeviceWaitIdle(deviceB_);
}