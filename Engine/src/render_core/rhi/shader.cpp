#include "shader.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu
{

std::shared_ptr<Shader> Shader::create(const Shader::Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanShader>(desc);
    }
}

} // namespace Mizu
