#pragma once

#include <cassert>

namespace Mizu::Vulkan
{

#ifdef MIZU_DEBUG

#define VK_CHECK(expression)                       \
    do                                             \
    {                                              \
        if ((expression) != VK_SUCCESS)            \
        {                                          \
            assert(false && "Vulkan call failed"); \
        }                                          \
    } while (false)

#else

#define VK_CHECK(expression) expression

#endif

} // namespace Mizu::Vulkan
