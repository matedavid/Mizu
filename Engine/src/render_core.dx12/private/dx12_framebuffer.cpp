#include "dx12_framebuffer.h"

#include "base/debug/assert.h"

#include "dx12_image_resource.h"
#include "dx12_resource_view.h"

namespace Mizu::Dx12
{

Dx12Framebuffer::Dx12Framebuffer(FramebufferDescription desc) : m_description(std::move(desc))
{
    MIZU_ASSERT(
        !m_description.color_attachments.is_empty() || m_description.depth_stencil_attachment.has_value(),
        "Empty framebuffer not allowed");
    MIZU_ASSERT(
        m_description.width > 0 && m_description.height > 0,
        "Framebuffer width and height must be greater than 0 (width = {}, height = {})",
        m_description.width,
        m_description.height);

    for (const FramebufferAttachment& attachment : m_description.color_attachments)
    {
        const Dx12ImageResourceView* internal_rtv = get_internal_image_resource_view(attachment.rtv);
        const ImageFormat format = internal_rtv->format;

        MIZU_ASSERT(!is_depth_format(format), "Can't use a rtv with a depth format as a color attachment");

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
        render_target_desc.cpuDescriptor = internal_rtv->handle;
        render_target_desc.BeginningAccess = beginning_access;
        render_target_desc.EndingAccess = ending_access;

        m_color_attachment_descriptions.push_back(render_target_desc);
    }

    if (m_description.depth_stencil_attachment.has_value())
    {
        const FramebufferAttachment& attachment = *m_description.depth_stencil_attachment;

        const Dx12ImageResourceView* internal_rtv = get_internal_image_resource_view(attachment.rtv);
        const ImageFormat format = internal_rtv->format;

        MIZU_ASSERT(is_depth_format(format), "Can't use a dsv with a format that is not compatible for depth stencil");

        m_depth_stencil_attachment_description.cpuDescriptor = internal_rtv->handle;

        D3D12_RENDER_PASS_BEGINNING_ACCESS depth_beginning_access{};
        depth_beginning_access.Type = Dx12Framebuffer::get_dx12_framebuffer_load_operation(attachment.load_operation);

        if (attachment.load_operation == LoadOperation::Clear)
        {
            depth_beginning_access.Clear.ClearValue.Format = Dx12ImageResource::get_dx12_image_format(format);
            depth_beginning_access.Clear.ClearValue.DepthStencil.Depth = attachment.clear_value.r;
        }

        D3D12_RENDER_PASS_ENDING_ACCESS depth_ending_access{};
        depth_ending_access.Type = Dx12Framebuffer::get_dx12_framebuffer_store_operation(attachment.store_operation);

        m_depth_stencil_attachment_description.DepthBeginningAccess = depth_beginning_access;
        m_depth_stencil_attachment_description.DepthEndingAccess = depth_ending_access;
        // TODO: stencils
        m_depth_stencil_attachment_description.StencilBeginningAccess.Type =
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        m_depth_stencil_attachment_description.StencilEndingAccess.Type =
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;

        m_has_depth_stencil_attachment = true;
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
    return std::span(m_color_attachment_descriptions.data(), m_color_attachment_descriptions.size());
}

std::optional<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> Dx12Framebuffer::get_depth_stencil_attachment_description() const
{
    if (m_has_depth_stencil_attachment)
        return m_depth_stencil_attachment_description;

    return {};
}

} // namespace Mizu::Dx12