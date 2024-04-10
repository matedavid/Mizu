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

  private:
    VkInstance m_instance;

    [[nodiscard]] bool validation_layers_available(const std::vector<const char*>& validation_layers);
};

} // namespace Mizu