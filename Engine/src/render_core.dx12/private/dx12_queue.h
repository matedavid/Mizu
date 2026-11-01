#pragma once

#include <memory>
#include <mutex>

#include "render_core/rhi/queue.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Fence;

class Dx12Queue : public Queue
{
  public:
    Dx12Queue(ID3D12CommandQueue* queue, CommandBufferType type);
    ~Dx12Queue() override;

    CommandBufferType get_type() const override { return m_type; }

    using Queue::submit;
    SyncPoint submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info) override;

    void wait(const SyncPoint& point) override;

    SyncPoint get_last_submitted() const override;
    void wait_idle() override;

    ID3D12CommandQueue* handle() const { return m_queue; }

  private:
    ID3D12CommandQueue* m_queue = nullptr;
    CommandBufferType m_type;

    std::unique_ptr<Dx12Fence> m_fence;
    uint64_t m_last_value = 0;

    mutable std::mutex m_mutex;
};

} // namespace Mizu::Dx12
