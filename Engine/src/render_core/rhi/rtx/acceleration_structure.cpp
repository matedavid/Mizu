#include "acceleration_structure.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_acceleration_structure.h"

namespace Mizu
{

std::shared_ptr<BottomLevelAccelerationStructure> BottomLevelAccelerationStructure::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBottomLevelAccelerationStructure>(desc);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Unimplemented");
    }
}

} // namespace Mizu