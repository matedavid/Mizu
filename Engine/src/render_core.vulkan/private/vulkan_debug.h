#pragma once

#include <glm/glm.hpp>

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

#if MIZU_VULKAN_VALIDATIONS_ENABLED

namespace debug
{

void init(VkInstance instance);

void begin_gpu_marker(VkCommandBuffer command_buffer, std::string_view label, glm::vec4 color = {});
void end_gpu_marker(VkCommandBuffer command_buffer);

void set_debug_name(uint64_t object, VkObjectType object_type, std::string_view name);

} // namespace debug

#define VK_DEBUG_INIT(instance) debug::init(instance)

#define VK_DEBUG_BEGIN_GPU_MARKER(cmd, label) debug::begin_gpu_marker(cmd, label)
#define VK_DEBUG_END_GPU_MARKER(cmd) debug::end_gpu_marker(cmd)

#define VK_DEBUG_SET_OBJECT_NAME(object, object_type, name) \
    debug::set_debug_name(reinterpret_cast<uint64_t>(object), object_type, name)

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
