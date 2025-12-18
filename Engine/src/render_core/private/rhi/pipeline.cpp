#include "render_core/rhi/pipeline.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Pipeline> Pipeline::create(const GraphicsPipelineDescription& desc)
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

std::shared_ptr<Pipeline> Pipeline::create(const ComputePipelineDescription& desc)
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

std::shared_ptr<Pipeline> Pipeline::create(const RayTracingPipelineDescription& desc)
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