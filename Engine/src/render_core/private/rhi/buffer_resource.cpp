#include "render_core/rhi/buffer_resource.h"

#include "base/debug/assert.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<BufferResource> BufferResource::create(const BufferDescription& desc)
{
    (void)desc;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

#if MIZU_DEBUG

std::string_view buffer_resource_state_to_string(BufferResourceState state)
{
    switch (state)
    {
    case BufferResourceState::Undefined:
        return "Undefined";
    case BufferResourceState::UnorderedAccess:
        return "UnorderedAccess";
    case BufferResourceState::TransferSrc:
        return "TransferSrc";
    case BufferResourceState::TransferDst:
        return "TransferDst";
    case BufferResourceState::ShaderReadOnly:
        return "ShaderReadOnly";
    }
}

#endif

//
// BufferUtils
//

void BufferUtils::initialize_buffer(const BufferResource& resource, const uint8_t* data, size_t size)
{
    BufferDescription staging_buffer_desc{};
    staging_buffer_desc.size = size;
    staging_buffer_desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;

    const auto staging_buffer = BufferResource::create(staging_buffer_desc);
    staging_buffer->set_data(data);

    RenderCommandBuffer::submit_single_time([&](CommandBuffer& command) {
        command.transition_resource(*staging_buffer, BufferResourceState::Undefined, BufferResourceState::TransferSrc);
        command.transition_resource(resource, BufferResourceState::Undefined, BufferResourceState::TransferDst);

        command.copy_buffer_to_buffer(*staging_buffer, resource);

        command.transition_resource(resource, BufferResourceState::TransferDst, BufferResourceState::ShaderReadOnly);
    });
}

void BufferUtils::initialize_image(const ImageResource& resource, const uint8_t* data)
{
    const ImageMemoryRequirements image_memory_reqs = resource.get_image_memory_requirements();

    BufferDescription staging_buffer_desc{};
    staging_buffer_desc.size = image_memory_reqs.size;
    staging_buffer_desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;

    const auto staging_buffer = BufferResource::create(staging_buffer_desc);

    const size_t format_size = ImageUtils::get_format_size(resource.get_format());
    const size_t row_size = resource.get_width() * format_size;

    for (uint32_t height = 0; height < resource.get_height(); ++height)
    {
        const uint8_t* src_data = data + height * row_size;

        const size_t offset = image_memory_reqs.offset + height * image_memory_reqs.row_pitch;
        staging_buffer->set_data(src_data, row_size, offset);
    }

    RenderCommandBuffer::submit_single_time([&](CommandBuffer& command) {
        command.transition_resource(*staging_buffer, BufferResourceState::Undefined, BufferResourceState::TransferSrc);
        command.transition_resource(resource, ImageResourceState::Undefined, ImageResourceState::TransferDst);

        command.copy_buffer_to_image(*staging_buffer, resource);

        command.transition_resource(resource, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
    });
}

} // namespace Mizu