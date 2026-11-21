#pragma once

#include "render_core/rhi/synchronization.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Fence : public Fence
{
  public:
    Dx12Fence(uint64_t initial_value);
    ~Dx12Fence() override;

    uint64_t get_completed_value() const override;
    void wait(uint64_t value) const override;
    void signal(uint64_t value) override;

    ID3D12Fence* handle() const { return m_handle; }

  private:
    ID3D12Fence* m_handle = nullptr;
};

} // namespace Mizu::Dx12