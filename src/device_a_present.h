#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "shared_image.h"
#include "gpu_timestamps.h"

enum RenderQueriesA : uint32_t {
  QA_BEGIN_CONSUME = 0,
  QA_END_COMPUTE   = 1,
  QA_BEGIN_COMPOSE = 2,
  QA_END_COMPOSE   = 3,
  QA_COUNT         = 4
};

class DeviceAPresent {
public:
  void init(VkPhysicalDevice phys, VkDevice dev,
            uint32_t graphicsQueueFamily,
            uint32_t computeQueueFamily,
            VkQueue graphicsQueue,
            VkQueue computeQueue,
            VkQueue presentQueue);

  void importSharedTargets(uint32_t frameCount,
                           const SharedImageCreateInfo& info,
                           const std::vector<ExportedImageHandle>& exported);

  void runComputePass(uint32_t slot, uint64_t timelineValue);
  void composeAndPresent(uint32_t slot);

  const GpuTimestamps& timestamps() const { return timestampsA_; }

private:
  VkPhysicalDevice phys_ = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;

  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  VkQueue computeQueue_ = VK_NULL_HANDLE;
  VkQueue presentQueue_ = VK_NULL_HANDLE;

  SharedImageCreateInfo imageInfo_{};
  std::vector<ImportedImageHandle> imports_;

  GpuTimestamps timestampsA_;
};