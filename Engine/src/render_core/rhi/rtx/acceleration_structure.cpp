#include "acceleration_structure.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_acceleration_structure.h"

namespace Mizu
{

std::shared_ptr<AccelerationStructure> AccelerationStructure::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanAccelerationStructure>(desc);
    }
}

} // namespace Mizu