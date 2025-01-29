#pragma once

#include <cassert>

#include "utility/assert.h"

namespace Mizu::Vulkan
{

#ifdef MIZU_DEBUG

#define VK_CHECK(expression) MIZU_ASSERT((expression) == VK_SUCCESS, "Vulkan call failed")

#else

#define VK_CHECK(expression) expression

#endif

} // namespace Mizu::Vulkan
