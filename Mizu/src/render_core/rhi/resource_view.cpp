#include "resource_view.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"

namespace Mizu
{

std::shared_ptr<ImageResourceView> ImageResourceView::create(std::shared_ptr<ImageResource> resource,
                                                             Range mip_range,
                                                             Range layer_range)
{
#if MIZU_DEBUG
    const uint32_t max_mip = mip_range.get_base() + mip_range.get_count() - 1;
    MIZU_ASSERT(mip_range.get_base() < resource->get_num_mips() && max_mip < resource->get_num_mips(),
                "Mip range is not valid (num_mips = {}, range = ({},{}))",
                resource->get_num_mips(),
                mip_range.get_base(),
                max_mip);

    const uint32_t max_layer = layer_range.get_base() + layer_range.get_count() - 1;
    MIZU_ASSERT(layer_range.get_base() < resource->get_num_layers() && max_layer < resource->get_num_layers(),
                "Layer range is not valid (num_layers = {}, range = ({},{}))",
                resource->get_num_layers(),
                layer_range.get_base(),
                max_layer);
#endif

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResourceView>(resource, mip_range, layer_range);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Unimplemented");
        return nullptr;
    }
}

} // namespace Mizu
