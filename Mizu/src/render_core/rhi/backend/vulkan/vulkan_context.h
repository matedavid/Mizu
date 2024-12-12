#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_device.h"
#include "render_core/rhi/backend/vulkan/vulkan_instance.h"

namespace Mizu::Vulkan
{

class VulkanDebug
{
  public:
    VulkanDebug() = delete;

    static void init(VkInstance instance);

    static void begin_label(VkCommandBuffer command_buffer, std::string_view label, glm::vec4 color = {});
    static void end_label(VkCommandBuffer command_buffer);

  private:
    static bool m_enabled;

    static PFN_vkCmdBeginDebugUtilsLabelEXT m_begin_label_internal;
    static PFN_vkCmdEndDebugUtilsLabelEXT m_end_label_internal;
};

#define VK_DEBUG_INIT(instance) VulkanDebug::init(instance)

#define VK_DEBUG_BEGIN_LABEL(cmd, label) VulkanDebug::begin_label(cmd, label)
#define VK_DEBUG_END_LABEL(cmd) VulkanDebug::end_label(cmd)

struct VulkanContextT
{
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
    std::unique_ptr<VulkanDescriptorLayoutCache> layout_cache;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan
