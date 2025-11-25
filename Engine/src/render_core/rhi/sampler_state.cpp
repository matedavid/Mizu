#include "sampler_state.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_sampler_state.h"
#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"

namespace Mizu
{

std::shared_ptr<SamplerState> SamplerState::create(const SamplingOptions& options)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12SamplerState>(options);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanSamplerState>(options);
    }
}

} // namespace Mizu