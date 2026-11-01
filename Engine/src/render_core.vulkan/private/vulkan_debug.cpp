#include "vulkan_debug.h"

#include "vulkan_context.h"

namespace Mizu::Vulkan
{

#if MIZU_VULKAN_VALIDATIONS_ENABLED

namespace debug
{

static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

void init(VkInstance instance)
{
    vkCmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    vkCmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
    vkSetDebugUtilsObjectNameEXT =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
}

void begin_gpu_marker(VkCommandBuffer command_buffer, std::string_view label, glm::vec4 color)
{
    if (!VulkanContext.validations_enabled)
        return;

    if (label.empty())
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

void end_gpu_marker(VkCommandBuffer command_buffer)
{
    if (!VulkanContext.validations_enabled)
        return;

    vkCmdEndDebugUtilsLabelEXT(command_buffer);
}

void set_debug_name(uint64_t object, VkObjectType object_type, std::string_view name)
{
    if (!VulkanContext.validations_enabled)
        return;

    if (name.empty())
        return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = object_type;
    info.objectHandle = object;
    info.pObjectName = name.data();

    vkSetDebugUtilsObjectNameEXT(VulkanContext.device->handle(), &info);
}

} // namespace debug

#endif

} // namespace Mizu::Vulkan
