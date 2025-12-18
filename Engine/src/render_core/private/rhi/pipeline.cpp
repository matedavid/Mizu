#include "render_core/rhi/pipeline.h"

#include "backend/dx12/dx12_pipeline.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Pipeline> Pipeline::create(const GraphicsPipelineDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Pipeline>(desc);
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<Pipeline> Pipeline::create(const ComputePipelineDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Pipeline>(desc);
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<Pipeline> Pipeline::create(const RayTracingPipelineDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Pipeline>(desc);
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu