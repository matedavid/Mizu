#include "ray_tracing_pipeline.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_ray_tracing_pipeline.h"

namespace Mizu
{

std::shared_ptr<RayTracingPipeline> RayTracingPipeline::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanRayTracingPipeline>(desc);
    }
}

} // namespace Mizu