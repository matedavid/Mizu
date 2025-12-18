#include "render_core/rhi/sampler_state.h"

#include "base/debug/assert.h"

#include "backend/dx12/dx12_sampler_state.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<SamplerState> SamplerState::create(const SamplerStateDescription& options)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12SamplerState>(options);
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu