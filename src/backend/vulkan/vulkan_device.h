#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "renderer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanInstance;

class VulkanDevice {
  public:
    VulkanDevice(const VulkanInstance& instance, const Requirements& reqs);
    ~VulkanDevice();

    [[nodiscard]] VkDevice handle() const { return m_device; }
    [[nodiscard]] VkPhysicalDevice physical_device() const { return m_physical_device; }

  private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};

    struct QueueFamilies {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    };
    QueueFamilies m_queue_families;

    void select_physical_device(const VulkanInstance& instance, const Requirements& reqs);

    [[nodiscard]] static VkPhysicalDeviceProperties get_properties(VkPhysicalDevice physical_device);
    [[nodiscard]] static std::vector<VkQueueFamilyProperties> get_queue_family_properties(
        VkPhysicalDevice physical_device);
};

} // namespace Mizu::Vulkan