#include "shader.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_shader.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

namespace Mizu
{

std::shared_ptr<Shader> Shader::create(const ShaderDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Shader>(desc);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanShader>(desc);
    }
}

} // namespace Mizu
