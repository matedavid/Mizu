#pragma once

#include "render_core/rhi/gpu_heap.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanGpuHeap : public GpuHeap
{
  public:
    VulkanGpuHeap(GpuHeapDescription desc);
    ~VulkanGpuHeap() override;

    bool is_compatible(const MemoryRequirements& reqs) const override;
    const GpuHeapDescription& get_description() const override { return m_description; }

    uint8_t* get_mapped_memory() const;

    VkDeviceMemory handle() const { return m_memory; }

  private:
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    uint8_t* m_mapped_memory = nullptr;

    GpuHeapDescription m_description{};
};

} // namespace Mizu::Vulkan
