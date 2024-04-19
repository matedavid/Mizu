#include "vulkan_backend.h"

#include <cassert>
#include <memory>

#include "backend/vulkan/vulkan_command_buffer.h"
#include "backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan {

bool VulkanBackend::initialize(const Configuration& config) {
    assert(std::holds_alternative<VulkanSpecificConfiguration>(config.backend_specific_config)
           && "backend_specific_configuration is not VulkanSpecificConfiguration");

    const auto& instance_extensions =
        std::get<VulkanSpecificConfiguration>(config.backend_specific_config).instance_extensions;

    VulkanContext.instance = std::make_unique<VulkanInstance>(VulkanInstance::Description{
        .application_name = config.application_name,
        .application_version = VK_MAKE_VERSION(
            config.application_version.major, config.application_version.minor, config.application_version.patch),
        .extensions = instance_extensions,
    });

    VulkanContext.device =
        std::make_unique<VulkanDevice>(*VulkanContext.instance, config.requirements, instance_extensions);

    VulkanContext.layout_cache = std::make_unique<VulkanDescriptorLayoutCache>();

    return true;
}

VulkanBackend::~VulkanBackend() {
    // NOTE: Order of destruction matters
    VulkanContext.layout_cache.reset();
    VulkanContext.device.reset();
    VulkanContext.instance.reset();
}

void VulkanBackend::draw(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const {
    const auto& native = dynamic_pointer_cast<IVulkanCommandBuffer>(command_buffer);
    vkCmdDraw(native->handle(), count, 1, 0, 0);
}

void VulkanBackend::draw_indexed(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const {
    const auto& native = dynamic_pointer_cast<IVulkanCommandBuffer>(command_buffer);
    vkCmdDrawIndexed(native->handle(), count, 1, 0, 0, 0);
}

} // namespace Mizu::Vulkan
