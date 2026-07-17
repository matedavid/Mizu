#include "render_core/rhi/render_pass.h"

namespace Mizu
{

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

} // namespace Mizu