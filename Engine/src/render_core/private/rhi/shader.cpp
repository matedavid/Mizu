#include "render_core/rhi/shader.h"

#include "backend/dx12/dx12_shader.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Shader> Shader::create(const ShaderDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Shader>(desc);
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu
