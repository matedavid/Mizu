#include "vulkan_context.h"

namespace Mizu::Vulkan
{

//
// VulkanDebug
//

bool VulkanDebug::m_enabled = false;
uint32_t VulkanDebug::m_active_labels = 0;

static PFN_vkCmdBeginDebugUtilsLabelEXT s_begin_label_internal;
static PFN_vkCmdEndDebugUtilsLabelEXT s_end_label_internal;
static PFN_vkSetDebugUtilsObjectNameEXT s_set_object_name;

void VulkanDebug::init(VkInstance instance)
{
    m_enabled = true;

    s_begin_label_internal =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    s_end_label_internal =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
    s_set_object_name =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
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

    s_begin_label_internal(command_buffer, &marker_info);

    m_active_labels++;
}

void VulkanDebug::end_label(VkCommandBuffer command_buffer)
{
    if (!m_enabled)
        return;

    if (m_active_labels == 0)
    {
        MIZU_LOG_WARNING("Trying to end debug label when no active label is active");
        return;
    }

    s_end_label_internal(command_buffer);

    m_active_labels--;
}

void VulkanDebug::set_debug_name(VkImage image, std::string_view name)
{
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = reinterpret_cast<uint64_t>(image);
    info.pObjectName = name.data();

    s_set_object_name(VulkanContext.device->handle(), &info);
}

void VulkanDebug::set_debug_name(VkBuffer buffer, std::string_view name)
{
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_BUFFER;
    info.objectHandle = reinterpret_cast<uint64_t>(buffer);
    info.pObjectName = name.data();

    s_set_object_name(VulkanContext.device->handle(), &info);
}

void VulkanDebug::set_debug_name(VkFramebuffer framebuffer, std::string_view name)
{
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
    info.objectHandle = reinterpret_cast<uint64_t>(framebuffer);
    info.pObjectName = name.data();

    s_set_object_name(VulkanContext.device->handle(), &info);
}

//
// VulkanContext
//

VulkanContextT VulkanContext{};

VulkanContextT::~VulkanContextT() = default;

} // namespace Mizu::Vulkan
