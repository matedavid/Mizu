#include "render_core/rhi/command_buffer.h"

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

void CommandBuffer::submit() const
{
    submit(CommandBufferSubmitInfo{});
}

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
    const BufferTransitionInfo transition_info = BufferTransitionInfo(
        old_state, new_state, size, offset, std::nullopt, std::nullopt, ResourceTransitionMode::Normal);

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
    const ImageTransitionInfo transition_info = ImageTransitionInfo(
        old_state, new_state, view_desc, std::nullopt, std::nullopt, ResourceTransitionMode::Normal);

    transition_resource(image, transition_info);
}

void CommandBuffer::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    const CopyBufferToBufferInfo info{
        .size = source.get_size(),
        .src_offset = 0,
        .dst_offset = 0,
    };

    copy_buffer_to_buffer(source, dest, info);
}

void CommandBuffer::copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const
{
    const CopyBufferToImageInfo info{
        .buffer_offset = 0,
        .buffer_row_length = 0,
        .buffer_image_height = 0,
        .image_subresource_layers =
            {
                .mip_level = 0,
                .base_array_layer = 0,
                .layer_count = image.get_num_layers(),
            },
        .image_offset = {0, 0, 0},
        .image_extent = {image.get_width(), image.get_height(), image.get_depth()},
    };

    copy_buffer_to_image(buffer, image, info);
}

void CommandBuffer::copy_image_to_buffer(const ImageResource& image, const BufferResource& buffer) const
{
    const CopyImageToBufferInfo info{
        .buffer_offset = 0,
        .buffer_row_length = 0,
        .buffer_image_height = 0,
        .image_subresource_layers =
            {
                .mip_level = 0,
                .base_array_layer = 0,
                .layer_count = image.get_num_layers(),
            },
        .image_offset = {0, 0, 0},
        .image_extent = {image.get_width(), image.get_height(), image.get_depth()},
    };

    copy_image_to_buffer(image, buffer, info);
}

} // namespace Mizu
