#include "vulkan_backend.h"

#include <cassert>
#include <memory>

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan
{

bool VulkanBackend::initialize(const RendererConfiguration& config)
{
    assert(std::holds_alternative<VulkanSpecificConfiguration>(config.backend_specific_config)
           && "backend_specific_configuration is not VulkanSpecificConfiguration");

    auto instance_extensions =
        std::get<VulkanSpecificConfiguration>(config.backend_specific_config).instance_extensions;

    // Enable Vulkan debug utils extension if available
    const bool debug_extension_enabled = VulkanInstance::is_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (debug_extension_enabled)
    {
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VulkanContext.instance = std::make_unique<VulkanInstance>(VulkanInstance::Description{
        .application_name = config.application_name,
        .application_version = VK_MAKE_VERSION(
            config.application_version.major, config.application_version.minor, config.application_version.patch),
        .extensions = instance_extensions,
    });

    if (debug_extension_enabled)
    {
        VK_DEBUG_INIT(VulkanContext.instance->handle());
    }

    VulkanContext.device =
        std::make_unique<VulkanDevice>(*VulkanContext.instance, config.requirements, instance_extensions);

    VulkanContext.layout_cache = std::make_unique<VulkanDescriptorLayoutCache>();

    return true;
}

VulkanBackend::~VulkanBackend()
{
    // NOTE: Order of destruction matters
    VulkanContext.layout_cache.reset();
    VulkanContext.device.reset();
    VulkanContext.instance.reset();
}

void VulkanBackend::wait_idle() const
{
    VK_CHECK(vkDeviceWaitIdle(VulkanContext.device->handle()));
}

} // namespace Mizu::Vulkan
