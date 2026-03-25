#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "shared_image.h"
#include "gpu_timestamps.h"

enum RenderQueriesB : uint32_t {
  QB_BEGIN_RENDER = 0,
  QB_END_RENDER   = 1,
  QB_COUNT        = 2
};

class DeviceBRender {
public:
  void init(
      VkPhysicalDevice phys,
      VkDevice dev,
      uint32_t graphicsQueueFamily,
      VkQueue graphicsQueue);

  void createSharedTargets(
      uint32_t frameCount,
      const SharedImageCreateInfo& info);

  void renderFrame(uint32_t slot, uint64_t frameId);

  const GpuTimestamps& timestamps() const { return timestampsB_; }

  const ExportedImageHandle& exported(uint32_t i) const {
    return exports_[i];
  }

private:
  VkPhysicalDevice phys_ = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;

  uint32_t queueFamily_ = 0;
  VkQueue queue_ = VK_NULL_HANDLE;

  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> cmdBuffers_;

  SharedImageCreateInfo imageInfo_{};
  std::vector<ExportedImageHandle> exports_;

  GpuTimestamps timestampsB_;
};