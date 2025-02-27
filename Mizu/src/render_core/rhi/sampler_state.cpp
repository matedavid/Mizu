#include "sampler_state.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"

namespace Mizu
{

std::shared_ptr<SamplerState> SamplerState::create(SamplingOptions options)
{
    std::shared_ptr<SamplerState> sampler_state;
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        sampler_state = std::make_shared<Vulkan::VulkanSamplerState>(options);
        break;
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Unimplemented");
        return nullptr;
    }

    return sampler_state;
}

} // namespace Mizu