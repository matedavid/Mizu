#include "render_core/rhi/acceleration_structure.h"

#include "backend/vulkan/vulkan_acceleration_structure.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<AccelerationStructure> AccelerationStructure::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanAccelerationStructure>(desc);
    }
}

} // namespace Mizu