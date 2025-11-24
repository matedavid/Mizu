#pragma once

#include <memory>

#include "render_core/rhi/resource_view.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12BufferResource;
class Dx12ImageResource;

class Dx12ShaderResourceView : public ShaderResourceView
{
  public:
    Dx12ShaderResourceView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range);
    Dx12ShaderResourceView(std::shared_ptr<BufferResource> resource);
    ~Dx12ShaderResourceView() override;

    D3D12_CPU_DESCRIPTOR_HANDLE handle() const { return m_handle; }

  private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_handle;
};

class Dx12UnorderedAccessView : public UnorderedAccessView
{
  public:
    Dx12UnorderedAccessView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range);
    Dx12UnorderedAccessView(std::shared_ptr<BufferResource> resource);
    ~Dx12UnorderedAccessView() override;

    D3D12_CPU_DESCRIPTOR_HANDLE handle() const { return m_handle; }

  private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_handle;
};

class Dx12ConstantBufferView : public ConstantBufferView
{
  public:
    Dx12ConstantBufferView(std::shared_ptr<BufferResource> resource);
    ~Dx12ConstantBufferView() override;

    D3D12_CPU_DESCRIPTOR_HANDLE handle() const { return m_handle; }

  private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_handle;
};

class Dx12RenderTargetView : public RenderTargetView
{
  public:
    Dx12RenderTargetView(std::shared_ptr<ImageResource> resource, ImageFormat format, ImageResourceViewRange range);
    ~Dx12RenderTargetView() override;

    ImageFormat get_format() const override { return m_format; }
    ImageResourceViewRange get_range() const override { return m_range; }

    D3D12_CPU_DESCRIPTOR_HANDLE handle() const { return m_handle; }

  private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_handle;

    ImageFormat m_format;
    ImageResourceViewRange m_range;
};

} // namespace Mizu::Dx12