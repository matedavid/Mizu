#include "buffer_resource.h"

#include "base/debug/assert.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"

namespace Mizu
{

std::shared_ptr<BufferResource> BufferResource::create(const BufferDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBufferResource>(desc);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    }
}

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

    TransferCommandBuffer::submit_single_time(
        [&](CommandBuffer& command) { command.copy_buffer_to_buffer(*staging_buffer, resource); });
}

void BufferUtils::initialize_image(const ImageResource& resource, const uint8_t* data, size_t size)
{
    BufferDescription staging_buffer_desc{};
    staging_buffer_desc.size = size;
    staging_buffer_desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;

    const auto staging_buffer = BufferResource::create(staging_buffer_desc);
    staging_buffer->set_data(data);

    RenderCommandBuffer::submit_single_time([&](CommandBuffer& command) {
        command.transition_resource(resource, ImageResourceState::Undefined, ImageResourceState::TransferDst);

        command.copy_buffer_to_image(*staging_buffer, resource);

        command.transition_resource(resource, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
    });
}

} // namespace Mizu