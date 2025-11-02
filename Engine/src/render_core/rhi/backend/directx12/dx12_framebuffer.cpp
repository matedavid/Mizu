#include "dx12_framebuffer.h"

#include "base/debug/assert.h"

#include "render_core/rhi/backend/directx12/dx12_image_resource.h"
#include "render_core/rhi/backend/directx12/dx12_resource_view.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu::Dx12
{

Dx12Framebuffer::Dx12Framebuffer(Description desc) : m_description(std::move(desc))
{
    MIZU_ASSERT(!m_description.attachments.empty(), "Empty framebuffer not allowed");
    MIZU_ASSERT(
        m_description.width > 0 && m_description.height > 0,
        "Framebuffer width and height must be greater than 0 (width = {}, height = {})",
        m_description.width,
        m_description.height);

    for (const Framebuffer::Attachment& attachment : m_description.attachments)
    {
        const Dx12RenderTargetView& native_rtv = dynamic_cast<const Dx12RenderTargetView&>(*attachment.rtv);
        const ImageFormat format = attachment.rtv->get_format();

        if (ImageUtils::is_depth_format(format))
        {
            MIZU_ASSERT(!m_has_depth_stencil_attachment, "Framebuffer should only have one depth stencil attachment");

            m_depth_stencil_attachment_description.cpuDescriptor = native_rtv.handle();

            D3D12_RENDER_PASS_BEGINNING_ACCESS depth_beginning_access{};
            depth_beginning_access.Type =
                Dx12Framebuffer::get_dx12_framebuffer_load_operation(attachment.load_operation);

            if (attachment.load_operation == LoadOperation::Clear)
            {
                depth_beginning_access.Clear.ClearValue.Format = Dx12ImageResource::get_dx12_image_format(format);

                depth_beginning_access.Clear.ClearValue.DepthStencil.Depth = 1.0f;
            }

            D3D12_RENDER_PASS_ENDING_ACCESS depth_ending_access{};
            depth_ending_access.Type =
                Dx12Framebuffer::get_dx12_framebuffer_store_operation(attachment.store_operation);

            m_depth_stencil_attachment_description.DepthBeginningAccess = depth_beginning_access;
            // TODO: m_depth_stencil_attachment_description.StencilBeginningAccess;
            m_depth_stencil_attachment_description.DepthEndingAccess = depth_ending_access;
            // TODO: m_depth_stencil_attachment_description.StencilEndingAccess;

            m_has_depth_stencil_attachment = true;
            continue;
        }

        D3D12_RENDER_PASS_BEGINNING_ACCESS beginning_access{};
        beginning_access.Type = Dx12Framebuffer::get_dx12_framebuffer_load_operation(attachment.load_operation);

        if (attachment.load_operation == LoadOperation::Clear)
        {
            beginning_access.Clear.ClearValue.Format = Dx12ImageResource::get_dx12_image_format(format);
            beginning_access.Clear.ClearValue.Color[0] = attachment.clear_value.r;
            beginning_access.Clear.ClearValue.Color[1] = attachment.clear_value.g;
            beginning_access.Clear.ClearValue.Color[2] = attachment.clear_value.b;
            beginning_access.Clear.ClearValue.Color[3] = attachment.clear_value.a;
        }

        D3D12_RENDER_PASS_ENDING_ACCESS ending_access{};
        ending_access.Type = Dx12Framebuffer::get_dx12_framebuffer_store_operation(attachment.store_operation);

        D3D12_RENDER_PASS_RENDER_TARGET_DESC render_target_desc{};
        render_target_desc.cpuDescriptor = native_rtv.handle();
        render_target_desc.BeginningAccess = beginning_access;
        render_target_desc.EndingAccess = ending_access;

        MIZU_ASSERT(m_num_color_attachments + 1 <= 8, "Max number of color attachments reached");
        m_color_attachment_descriptions[m_num_color_attachments++] = render_target_desc;
    }
}

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE Dx12Framebuffer::get_dx12_framebuffer_load_operation(LoadOperation operation)
{
    switch (operation)
    {
    case LoadOperation::Load:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    case LoadOperation::Clear:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    case LoadOperation::DontCare:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
    }
}

D3D12_RENDER_PASS_ENDING_ACCESS_TYPE Dx12Framebuffer::get_dx12_framebuffer_store_operation(StoreOperation operation)
{
    switch (operation)
    {
    case StoreOperation::Store:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    case StoreOperation::DontCare:
    case StoreOperation::None:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
    }
}

std::span<const D3D12_RENDER_PASS_RENDER_TARGET_DESC> Dx12Framebuffer::get_color_attachment_descriptions() const
{
    return std::span(m_color_attachment_descriptions.data(), m_num_color_attachments);
}

std::optional<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> Dx12Framebuffer::get_depth_stencil_attachment_description() const
{
    if (m_has_depth_stencil_attachment)
        return m_depth_stencil_attachment_description;

    return {};
}

} // namespace Mizu::Dx12