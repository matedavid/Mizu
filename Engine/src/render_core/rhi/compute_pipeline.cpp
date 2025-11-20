#include "compute_pipeline.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_compute_pipeline.h"
#include "render_core/rhi/backend/vulkan/vulkan_compute_pipeline.h"

namespace Mizu
{

std::shared_ptr<ComputePipeline> ComputePipeline::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12ComputePipeline>(desc);
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputePipeline>(desc);
    }
}

} // namespace Mizu