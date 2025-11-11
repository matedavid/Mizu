#pragma once

#include "render_core/rhi/sampler_state.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12SamplerState : public SamplerState
{
  public:
    Dx12SamplerState(SamplingOptions options);
    ~Dx12SamplerState() override;

    static D3D12_FILTER get_dx12_filter(ImageFilter minification, ImageFilter magnification_filter);
    static D3D12_TEXTURE_ADDRESS_MODE get_dx12_sampler_address_mode(ImageAddressMode mode);

    D3D12_CPU_DESCRIPTOR_HANDLE handle() const { return m_handle; }

  private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_handle;
    ID3D12DescriptorHeap* m_descriptor_heap = nullptr;

    SamplingOptions m_options{};
};

} // namespace Mizu::Dx12