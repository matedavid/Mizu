#include "render_core/rhi/shader.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Shader> Shader::create(const ShaderDescription& desc)
{
    (void)desc;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu
