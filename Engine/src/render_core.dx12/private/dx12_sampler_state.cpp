#include "dx12_sampler_state.h"

#include "dx12_context.h"

namespace Mizu::Dx12
{

Dx12SamplerState::Dx12SamplerState(SamplerStateDescription options) : m_options(std::move(options))
{
    m_handle = Dx12Context.sampler_heap->allocate();

    D3D12_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = get_dx12_filter(m_options.minification_filter, m_options.magnification_filter);
    sampler_desc.AddressU = get_dx12_sampler_address_mode(m_options.address_mode_u);
    sampler_desc.AddressV = get_dx12_sampler_address_mode(m_options.address_mode_v);
    sampler_desc.AddressW = get_dx12_sampler_address_mode(m_options.address_mode_w);
    sampler_desc.MipLODBias = 0.0f;
    sampler_desc.MaxAnisotropy = 0;
    sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler_desc.BorderColor[0] = 0.0f;
    sampler_desc.BorderColor[1] = 0.0f;
    sampler_desc.BorderColor[2] = 0.0f;
    sampler_desc.BorderColor[3] = 0.0f;
    sampler_desc.MinLOD = 0.0f;
    sampler_desc.MaxLOD = 1.0f;

    Dx12Context.device->handle()->CreateSampler(&sampler_desc, m_handle);
}

Dx12SamplerState::~Dx12SamplerState()
{
    Dx12Context.sampler_heap->free(m_handle);
}

D3D12_FILTER Dx12SamplerState::get_dx12_filter(ImageFilter minification, ImageFilter magnification_filter)
{
    if (minification == ImageFilter::Nearest && magnification_filter == ImageFilter::Nearest)
    {
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
    else if (minification == ImageFilter::Linear && magnification_filter == ImageFilter::Linear)
    {
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }
    else if (minification == ImageFilter::Linear && magnification_filter == ImageFilter::Nearest)
    {
        return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    }
    else // minification == ImageFilter::Nearest && magnification_filter == ImageFilter::Linear
    {
        return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    }
}

D3D12_TEXTURE_ADDRESS_MODE Dx12SamplerState::get_dx12_sampler_address_mode(ImageAddressMode mode)
{
    switch (mode)
    {
    case ImageAddressMode::Repeat:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case ImageAddressMode::MirroredRepeat:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case ImageAddressMode::ClampToEdge:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case ImageAddressMode::ClampToBorder:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    }
}

} // namespace Mizu::Dx12