#include "vulkan_debug.h"

#include "vulkan_context.h"

namespace Mizu::Vulkan
{

#if MIZU_VULKAN_VALIDATIONS_ENABLED

static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

void VulkanDebug::init(VkInstance instance)
{
    vkCmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    vkCmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
    vkSetDebugUtilsObjectNameEXT =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
}

void VulkanDebug::begin_gpu_marker(VkCommandBuffer command_buffer, std::string_view label, glm::vec4 color)
{
    if (!VulkanContext.validations_enabled)
        return;

    VkDebugUtilsLabelEXT marker_info{};
    marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    marker_info.pLabelName = label.data();

    marker_info.color[0] = color.r;
    marker_info.color[1] = color.g;
    marker_info.color[2] = color.b;
    marker_info.color[3] = color.a;

    vkCmdBeginDebugUtilsLabelEXT(command_buffer, &marker_info);
}

void VulkanDebug::end_gpu_marker(VkCommandBuffer command_buffer)
{
    if (!VulkanContext.validations_enabled)
        return;

    vkCmdEndDebugUtilsLabelEXT(command_buffer);
}

void VulkanDebug::set_debug_name(VkImage image, std::string_view name)
{
    if (!VulkanContext.validations_enabled)
        return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = reinterpret_cast<uint64_t>(image);
    info.pObjectName = name.data();

    vkSetDebugUtilsObjectNameEXT(VulkanContext.device->handle(), &info);
}

void VulkanDebug::set_debug_name(VkBuffer buffer, std::string_view name)
{
    if (!VulkanContext.validations_enabled)
        return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_BUFFER;
    info.objectHandle = reinterpret_cast<uint64_t>(buffer);
    info.pObjectName = name.data();

    vkSetDebugUtilsObjectNameEXT(VulkanContext.device->handle(), &info);
}

void VulkanDebug::set_debug_name(VkFramebuffer framebuffer, std::string_view name)
{
    if (!VulkanContext.validations_enabled)
        return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
    info.objectHandle = reinterpret_cast<uint64_t>(framebuffer);
    info.pObjectName = name.data();

    vkSetDebugUtilsObjectNameEXT(VulkanContext.device->handle(), &info);
}

void VulkanDebug::set_debug_name(VkAccelerationStructureKHR acceleration_structure, std::string_view name)
{
    if (!VulkanContext.validations_enabled)
        return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
    info.objectHandle = reinterpret_cast<uint64_t>(acceleration_structure);
    info.pObjectName = name.data();

    vkSetDebugUtilsObjectNameEXT(VulkanContext.device->handle(), &info);
}
void VulkanDebug::set_debug_name(VkDeviceMemory memory, std::string_view name)
{
    if (!VulkanContext.validations_enabled)
        return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
    info.objectHandle = reinterpret_cast<uint64_t>(memory);
    info.pObjectName = name.data();

    vkSetDebugUtilsObjectNameEXT(VulkanContext.device->handle(), &info);
}
#endif

} // namespace Mizu::Vulkan
