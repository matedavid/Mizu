#include "render_core/rhi/sampler_state.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<SamplerState> SamplerState::create(const SamplerStateDescription& options)
{
    (void)options;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu