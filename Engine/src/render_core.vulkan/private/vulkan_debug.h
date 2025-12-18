#pragma once

#include <glm/glm.hpp>

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

#if MIZU_VULKAN_VALIDATIONS_ENABLED

namespace VulkanDebug
{

void init(VkInstance instance);

void begin_gpu_marker(VkCommandBuffer command_buffer, std::string_view label, glm::vec4 color = {});
void end_gpu_marker(VkCommandBuffer command_buffer);

void set_debug_name(VkImage image, std::string_view name);
void set_debug_name(VkBuffer buffer, std::string_view name);
void set_debug_name(VkFramebuffer framebuffer, std::string_view name);
void set_debug_name(VkAccelerationStructureKHR acceleration_structure, std::string_view name);
void set_debug_name(VkDeviceMemory memory, std::string_view name);

} // namespace VulkanDebug

#define VK_DEBUG_INIT(instance) VulkanDebug::init(instance)

#define VK_DEBUG_BEGIN_GPU_MARKER(cmd, label) VulkanDebug::begin_gpu_marker(cmd, label)
#define VK_DEBUG_END_GPU_MARKER(cmd) VulkanDebug::end_gpu_marker(cmd)

#define VK_DEBUG_SET_OBJECT_NAME(object, name) VulkanDebug::set_debug_name(object, name)

#else

#define VK_DEBUG_INIT(instance) (void)instance

#define VK_DEBUG_BEGIN_GPU_MARKER(cmd, label) \
    do                                        \
    {                                         \
        (void)cmd;                            \
        (void)label;                          \
    } while (false)
#define VK_DEBUG_END_GPU_MARKER(cmd) \
    do                               \
    {                                \
        (void)cmd;                   \
    } while (false)

#define VK_DEBUG_SET_OBJECT_NAME(object, name) \
    do                                         \
    {                                          \
        (void)object;                          \
        (void)name;                            \
    } while (false)

#endif

} // namespace Mizu::Vulkan
