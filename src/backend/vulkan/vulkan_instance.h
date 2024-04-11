#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Mizu::Vulkan {

class VulkanInstance {
  public:
    struct Description {
        std::string application_name;
        uint32_t application_version;
    };

    explicit VulkanInstance(const Description& desc);
    ~VulkanInstance();

    [[nodiscard]] std::vector<VkPhysicalDevice> get_physical_devices() const;

  private:
    VkInstance m_handle{VK_NULL_HANDLE};

    [[nodiscard]] static bool validation_layers_available(const std::vector<const char*>& validation_layers);
};

} // namespace Mizu