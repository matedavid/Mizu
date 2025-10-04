#pragma once

#include <cassert>
#include <vulkan/vulkan.h>

#include "base/debug/assert.h"

namespace Mizu::Vulkan
{

#define MIZU_ENABLE_VULKAN_VALIDATIONS MIZU_DEBUG

#if MIZU_ENABLE_VULKAN_VALIDATIONS

inline std::string vulkan_result_to_string(const VkResult res)
{
    switch (res)
    {
    case VK_SUCCESS:
        break;
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_PIPELINE_COMPILE_REQUIRED:
        return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
        return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
    default:
        return "MIZU_VK_ERROR_NOT_IMPLEMENTED";
    }

    return "MIZU_VK_ERROR_NOT_IMPLEMENTED";
}

#define VK_CHECK_RESULT(expression)                                \
    do                                                             \
    {                                                              \
        const VkResult _vk_check_res = expression;                 \
        MIZU_ASSERT(                                               \
            _vk_check_res == VK_SUCCESS,                           \
            "Vulkan call failed with: {}",                         \
            Mizu::Vulkan::vulkan_result_to_string(_vk_check_res)); \
    } while (false)

#define VK_CHECK(expression) VK_CHECK_RESULT(expression)

#else

#define VK_CHECK_RESULT(expression) (void)expression;

#define VK_CHECK(expression) expression

#endif

} // namespace Mizu::Vulkan
