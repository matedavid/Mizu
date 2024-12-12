#include "vulkan_context.h"

namespace Mizu::Vulkan
{

//
// VulkanDebug
//

bool VulkanDebug::m_enabled = false;

PFN_vkCmdBeginDebugUtilsLabelEXT VulkanDebug::m_begin_label_internal;
PFN_vkCmdEndDebugUtilsLabelEXT VulkanDebug::m_end_label_internal;

void VulkanDebug::init(VkInstance instance)
{
    m_enabled = true;

    m_begin_label_internal =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    m_end_label_internal =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
}

void VulkanDebug::begin_label(VkCommandBuffer command_buffer, std::string_view label, glm::vec4 color)
{
    if (!m_enabled)
        return;

    VkDebugUtilsLabelEXT marker_info{};
    marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    marker_info.pLabelName = label.data();

    marker_info.color[0] = color.r;
    marker_info.color[1] = color.g;
    marker_info.color[2] = color.b;
    marker_info.color[3] = color.a;

    m_begin_label_internal(command_buffer, &marker_info);
}

void VulkanDebug::end_label(VkCommandBuffer command_buffer)
{
    if (!m_enabled)
        return;

    m_end_label_internal(command_buffer);
}

//
// VulkanContext
//

VulkanContextT VulkanContext{};

VulkanContextT::~VulkanContextT() = default;

} // namespace Mizu::Vulkan