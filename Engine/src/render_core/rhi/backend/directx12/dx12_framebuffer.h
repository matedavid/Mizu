#pragma once

#include <array>
#include <optional>
#include <span>

#include "render_core/rhi/framebuffer.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12Framebuffer : public Framebuffer
{
  public:
    Dx12Framebuffer(Description desc);

    std::span<const Attachment> get_attachments() const override { return std::span(m_description.attachments); }

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }

    static D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE get_dx12_framebuffer_load_operation(LoadOperation operation);
    static D3D12_RENDER_PASS_ENDING_ACCESS_TYPE get_dx12_framebuffer_store_operation(StoreOperation operation);

    std::span<const D3D12_RENDER_PASS_RENDER_TARGET_DESC> get_color_attachment_descriptions() const;
    std::optional<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> get_depth_stencil_attachment_description() const;

  private:
    Description m_description;

    std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, 8> m_color_attachment_descriptions;
    uint32_t m_num_color_attachments = 0;

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC m_depth_stencil_attachment_description{};
    bool m_has_depth_stencil_attachment = false;
};

} // namespace Mizu::Dx12