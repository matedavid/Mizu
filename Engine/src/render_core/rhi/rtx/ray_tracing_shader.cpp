#include "ray_tracing_shader.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_ray_tracing_shader.h"

namespace Mizu
{

std::shared_ptr<RayTracingShader> RayTracingShader::create(const RayTracingShaderStageInfo& info)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRayTracingShader>(info);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    }
}

} // namespace Mizu