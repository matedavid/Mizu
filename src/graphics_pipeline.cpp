#include "graphics_pipeline.h"

#include "renderer.h"

#include "backend/vulkan/vulkan_graphics_pipeline.h"

namespace Mizu {

std::shared_ptr<GraphicsPipeline> GraphicsPipeline::create(const Description& desc) {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanGraphicsPipeline>(desc);
    }
}

} // namespace Mizu
