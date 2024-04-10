#include "vulkan_backend.h"

#include <memory>

#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_instance.h"

namespace Mizu::Vulkan {

bool VulkanBackend::initialize(const Configuration& config) {
    VulkanContext.instance = std::make_unique<VulkanInstance>(VulkanInstance::Description{
        .application_name = config.application_name,
        .application_version = VK_MAKE_VERSION(
            config.application_version.major, config.application_version.minor, config.application_version.patch),
    });

    return true;
}

VulkanBackend::~VulkanBackend() {}

} // namespace Mizu::Vulkan