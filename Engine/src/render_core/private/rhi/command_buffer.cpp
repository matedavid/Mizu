#include "render_core/rhi/command_buffer.h"

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

void CommandBuffer::transition_resource(
    const BufferResource& buffer,
    BufferResourceState old_state,
    BufferResourceState new_state) const
{
    transition_resource(buffer, old_state, new_state, buffer.get_size(), 0);
}

void CommandBuffer::transition_resource(
    const BufferResource& buffer,
    BufferResourceState old_state,
    BufferResourceState new_state,
    size_t size,
    size_t offset) const
{
    const BufferTransitionInfo transition_info =
        BufferTransitionInfo(old_state, new_state, size, offset, std::nullopt, std::nullopt);

    transition_resource(buffer, transition_info);
}

void CommandBuffer::transition_resource(
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state) const
{
    const ImageResourceViewDescription view_desc = {
        .mip_base = 0,
        .mip_count = image.get_num_mips(),
        .layer_base = 0,
        .layer_count = image.get_num_layers(),
    };

    transition_resource(image, old_state, new_state, view_desc);
}

void CommandBuffer::transition_resource(
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state,
    ImageResourceViewDescription view_desc) const
{
    const ImageTransitionInfo transition_info =
        ImageTransitionInfo(old_state, new_state, view_desc, std::nullopt, std::nullopt);

    transition_resource(image, transition_info);
}

void CommandBuffer::submit() const
{
    submit(CommandBufferSubmitInfo{});
}

} // namespace Mizu
