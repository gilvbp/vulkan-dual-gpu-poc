#include "gpu_timestamps.h"
#include <stdexcept>

void GpuTimestamps::init(VkPhysicalDevice phys, VkDevice dev, uint32_t queryCount) {
    phys_ = phys;
    dev_ = dev;
    queryCount_ = queryCount;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys_, &props);
    timestampPeriod_ = props.limits.timestampPeriod;

    VkQueryPoolCreateInfo qpci{};
    qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpci.queryCount = queryCount_;

    if (vkCreateQueryPool(dev_, &qpci, nullptr, &pool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateQueryPool failed");
    }
}

void GpuTimestamps::destroy() {
    if (pool_) {
        vkDestroyQueryPool(dev_, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
}

void GpuTimestamps::reset(VkCommandBuffer cmd, uint32_t firstQuery, uint32_t queryCount) {
    vkCmdResetQueryPool(cmd, pool_, firstQuery, queryCount);
}

void GpuTimestamps::write2(VkCommandBuffer cmd, VkPipelineStageFlags2 stage, uint32_t queryIndex) {
    auto fn = reinterpret_cast<PFN_vkCmdWriteTimestamp2>(
        vkGetDeviceProcAddr(dev_, "vkCmdWriteTimestamp2"));
    if (!fn) {
        throw std::runtime_error("vkCmdWriteTimestamp2 not available");
    }
    fn(cmd, stage, pool_, queryIndex);
}

TimestampReadback GpuTimestamps::readOne(uint32_t queryIndex) const {
    struct QueryValue {
        uint64_t value;
        uint64_t available;
    } out{};

    VkResult r = vkGetQueryPoolResults(
        dev_,
        pool_,
        queryIndex,
        1,
        sizeof(out),
        &out,
        sizeof(out),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

    if (r != VK_SUCCESS && r != VK_NOT_READY) {
        throw std::runtime_error("vkGetQueryPoolResults failed");
    }

    return TimestampReadback{.value = out.value, .available = (out.available != 0)};
}

double GpuTimestamps::ticksToNanoseconds(uint64_t ticks) const {
    return static_cast<double>(ticks) * static_cast<double>(timestampPeriod_);
}

double GpuTimestamps::ticksToMilliseconds(uint64_t ticks) const {
    return ticksToNanoseconds(ticks) / 1'000'000.0;
}
