#include "sampler_state.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"

namespace Mizu
{

std::shared_ptr<SamplerState> SamplerState::create(SamplingOptions options)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSamplerState>(options);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Unimplemented");
        return nullptr;
    }
}

} // namespace Mizu