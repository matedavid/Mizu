#pragma once

#include "render_core/rhi/gpu_heap.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

// clang-format off
constexpr uint32_t MEMORY_TYPE_BITS_BUFFER        = (1 << 0);
constexpr uint32_t MEMORY_TYPE_BITS_TEXTURE       = (1 << 1);
constexpr uint32_t MEMORY_TYPE_BITS_RT_DS_TEXTURE = (1 << 2);
constexpr uint32_t MEMORY_TYPE_BITS_ALL           = std::numeric_limits<uint32_t>::max();
// clang-format on

class Dx12GpuHeap : public GpuHeap
{
  public:
    Dx12GpuHeap(GpuHeapDescription desc);
    ~Dx12GpuHeap() override;

    bool is_compatible(const MemoryRequirements& reqs) const override;
    const GpuHeapDescription& get_description() const override { return m_description; }

    ID3D12Heap* handle() const { return m_heap; }

  private:
    ID3D12Heap* m_heap = nullptr;

    GpuHeapDescription m_description{};
};

} // namespace Mizu::Dx12
