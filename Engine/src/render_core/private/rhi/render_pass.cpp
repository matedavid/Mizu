#include "render_core/rhi/render_pass.h"

#include "base/debug/assert.h"

namespace Mizu
{

static glm::uvec2 get_attachment_extent(const FramebufferAttachment& attachment)
{
    const ImageResource& image = *attachment.rtv.image;
    return compute_mip_size(image.get_width(), image.get_height(), attachment.rtv.desc.mip_base);
}

FramebufferInfo create_framebuffer_info(const RenderPassInfo& info)
{
    FramebufferInfo framebuffer_info{};

    for (const FramebufferAttachment& attachment : info.color_attachments)
    {
        const ImageFormat image_format = attachment.rtv.image->get_format();
        framebuffer_info.color_attachments.push_back(attachment.rtv.desc.override_format.value_or(image_format));
    }

    if (info.depth_stencil_attachment.has_value())
    {
        const ImageFormat image_format = info.depth_stencil_attachment->rtv.image->get_format();
        framebuffer_info.depth_stencil_attachment =
            info.depth_stencil_attachment->rtv.desc.override_format.value_or(image_format);
    }

    return framebuffer_info;
}

glm::uvec2 get_render_pass_extent(const RenderPassInfo& info)
{
    MIZU_ASSERT(
        !info.color_attachments.empty() || info.depth_stencil_attachment.has_value(),
        "RenderPass must have at least one attachment");

    const glm::uvec2 extent = !info.color_attachments.empty()
                                  ? get_attachment_extent(info.color_attachments[0])
                                  : get_attachment_extent(*info.depth_stencil_attachment);

#if MIZU_DEBUG
    for (const FramebufferAttachment& attachment : info.color_attachments)
    {
        MIZU_ASSERT(get_attachment_extent(attachment) == extent, "All RenderPass attachments must have the same size");
    }

    if (info.depth_stencil_attachment.has_value())
    {
        MIZU_ASSERT(
            get_attachment_extent(*info.depth_stencil_attachment) == extent,
            "All RenderPass attachments must have the same size");
    }
#endif

    return extent;
}

} // namespace Mizu