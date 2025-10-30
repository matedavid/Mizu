#pragma once

#include "render_core/rhi/synchronization.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Fence : public Fence
{
  public:
    Dx12Fence(bool signaled);
    ~Dx12Fence() override;

    void wait_for() override;
    void signal(ID3D12CommandQueue* command_queue);

  private:
    ID3D12Fence* m_handle = nullptr;
    HANDLE m_event;
    uint64_t m_counter = 0;

    // Hacky members to make it work how vulkan fences work
    bool m_signal_registered = false;
    bool m_first_wait_for = true;
};

} // namespace Mizu::Dx12