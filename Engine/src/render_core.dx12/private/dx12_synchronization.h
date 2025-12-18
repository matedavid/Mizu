#pragma once

#include "render_core/rhi/synchronization.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Fence : public Fence
{
  public:
    Dx12Fence(bool signaled);
    ~Dx12Fence() override;

    void wait_for() override;
    void signal(ID3D12CommandQueue* queue);

  private:
    ID3D12Fence* m_handle = nullptr;
    HANDLE m_event;

    uint64_t m_next_value = 1;
    uint64_t m_last_signaled_value = 0;
};

class Dx12Semaphore : public Semaphore
{
  public:
    Dx12Semaphore();
    ~Dx12Semaphore() override;

    void signal(ID3D12CommandQueue* queue);
    void wait(ID3D12CommandQueue* queue);

  private:
    ID3D12Fence* m_handle = nullptr;
    uint64_t m_counter = 0;
};

} // namespace Mizu::Dx12