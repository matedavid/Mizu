#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "render_core/rhi/gpu_heap.h"
#include "render_core/rhi/memory.h"

#include "mizu_render_module.h"

namespace Mizu
{

class MIZU_RENDER_API RenderGraphTransientMemory
{
  public:
    RenderGraphTransientMemory(std::string name = "RenderGraphTransientMemory");

    RenderGraphTransientMemory(const RenderGraphTransientMemory&) = delete;
    RenderGraphTransientMemory& operator=(const RenderGraphTransientMemory&) = delete;

    GpuHeap* get_heap() const { return m_heap.get(); }

    bool needs_recreate(MemoryTypeId memory_type, uint64_t size) const;
    GpuHeap& recreate(MemoryTypeId memory_type, uint64_t size);

  private:
    std::string m_name;
    std::shared_ptr<GpuHeap> m_heap;
};

} // namespace Mizu
