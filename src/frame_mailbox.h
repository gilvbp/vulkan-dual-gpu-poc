#pragma once
#include <atomic>
#include <cstdint>

struct FrameMailbox {
    std::atomic<uint64_t> latestRenderedFrame{0};
    std::atomic<uint32_t> latestRenderedSlot{0};
    std::atomic<uint64_t> latestConsumedFrame{0};

    void publishRendered(uint64_t frameId, uint32_t slot) {
        latestRenderedSlot.store(slot, std::memory_order_release);
        latestRenderedFrame.store(frameId, std::memory_order_release);
    }

    bool tryAcquireLatest(uint64_t& outFrameId, uint32_t& outSlot) {
        const uint64_t rendered = latestRenderedFrame.load(std::memory_order_acquire);
        const uint64_t consumed = latestConsumedFrame.load(std::memory_order_acquire);
        if (rendered == 0 || rendered == consumed) {
            return false;
        }
        outSlot = latestRenderedSlot.load(std::memory_order_acquire);
        outFrameId = rendered;
        return true;
    }

    void markConsumed(uint64_t frameId) {
        latestConsumedFrame.store(frameId, std::memory_order_release);
    }
};
