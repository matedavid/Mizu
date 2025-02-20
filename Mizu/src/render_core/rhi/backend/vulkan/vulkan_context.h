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

    static void set_debug_name(VkImage image, std::string_view name);
    static void set_debug_name(VkBuffer buffer, std::string_view name);
    static void set_debug_name(VkFramebuffer framebuffer, std::string_view name);

  private:
    static bool m_enabled;
    static uint32_t m_active_labels;
};

#define VK_DEBUG_INIT(instance) VulkanDebug::init(instance)

#define VK_DEBUG_BEGIN_LABEL(cmd, label) VulkanDebug::begin_label(cmd, label)
#define VK_DEBUG_END_LABEL(cmd) VulkanDebug::end_label(cmd)

#define VK_DEBUG_SET_OBJECT_NAME(object, name) VulkanDebug::set_debug_name(object, name)

struct VulkanContextT
{
    ~VulkanContextT();

    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanDevice> device;
    std::unique_ptr<VulkanDescriptorLayoutCache> layout_cache;
};

extern VulkanContextT VulkanContext;

} // namespace Mizu::Vulkan
