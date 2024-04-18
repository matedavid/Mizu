#include "vulkan_backend.h"

#include <memory>

#include "backend/vulkan/vulkan_command_buffer.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_device.h"
#include "backend/vulkan/vulkan_instance.h"

namespace Mizu::Vulkan {

bool VulkanBackend::initialize(const Configuration& config) {
    VulkanContext.instance = std::make_unique<VulkanInstance>(VulkanInstance::Description{
        .application_name = config.application_name,
        .application_version = VK_MAKE_VERSION(
            config.application_version.major, config.application_version.minor, config.application_version.patch),
    });

    VulkanContext.device = std::make_unique<VulkanDevice>(*VulkanContext.instance, config.requirements);

    return true;
}

VulkanBackend::~VulkanBackend() {
    // NOTE: Order of destruction matters
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
