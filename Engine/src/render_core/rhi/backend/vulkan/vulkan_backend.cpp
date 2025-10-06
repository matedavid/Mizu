#include "vulkan_backend.h"

#include <cassert>
#include <memory>

#include "base/debug/assert.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"

namespace Mizu::Vulkan
{

bool VulkanBackend::initialize(const RendererConfiguration& config)
{
    MIZU_ASSERT(
        std::holds_alternative<VulkanSpecificConfiguration>(config.backend_specific_config),
        "backend_specific_configuration is not VulkanSpecificConfiguration");

    auto instance_extensions =
        std::get<VulkanSpecificConfiguration>(config.backend_specific_config).instance_extensions;

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    // Enable Vulkan debug utils extension if available
    const bool debug_extension_enabled = VulkanInstance::is_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#else
    const bool debug_extension_enabled = false;
#endif

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

    VulkanContext.device = std::make_unique<VulkanDevice>(*VulkanContext.instance, instance_extensions);

    VulkanContext.layout_cache = std::make_unique<VulkanDescriptorLayoutCache>();

    VulkanDescriptorPool::PoolSize pool_size = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 5},
    };

    if (get_capabilities().ray_tracing_hardware)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 5);

    VulkanContext.descriptor_pool = std::make_unique<VulkanDescriptorPool>(pool_size, 100, true);

    if (get_capabilities().ray_tracing_hardware)
    {
        initialize_rtx(VulkanContext.device->handle());
    }

    return true;
}

VulkanBackend::~VulkanBackend()
{
    // NOTE: Order of destruction matters
    VulkanContext.descriptor_pool.reset();
    VulkanContext.layout_cache.reset();
    VulkanContext.device.reset();
    VulkanContext.instance.reset();
}

void VulkanBackend::wait_idle() const
{
    std::lock_guard mutex(m_mutex);
    VK_CHECK(vkDeviceWaitIdle(VulkanContext.device->handle()));
}

RendererCapabilities VulkanBackend::get_capabilities() const
{
    MIZU_ASSERT(VulkanContext.device != nullptr, "You must first call initialize()");

    return VulkanContext.device->get_physical_device_capabilities();
}

} // namespace Mizu::Vulkan
