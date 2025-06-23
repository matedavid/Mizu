#pragma once

#ifdef MIZU_DEBUG

#include <cassert>

#include "base/debug/logging.h"

namespace Mizu
{

#if MIZU_PLATFORM_WINDOWS

#define MIZU_DEBUG_BREAK() __debugbreak()

#elif MIZU_PLATFORM_UNIX

#define MIZU_DEBUG_BREAK() __builtin_trap()

#else

#define MIZU_DEBUG_BREAK() \
    do                     \
    {                      \
    } while (false)

#endif

// Only fails on Debug
#define MIZU_ASSERT(cond, ...)           \
    do                                   \
    {                                    \
        if (!(cond))                     \
        {                                \
            MIZU_LOG_ERROR(__VA_ARGS__); \
            MIZU_DEBUG_BREAK();          \
        }                                \
    } while (false)

// Fails also on release
#define MIZU_VERIFY(cond, ...) MIZU_ASSERT(cond, __VA_ARGS__)

#define MIZU_UNREACHABLE(...) MIZU_VERIFY(false, __VA_ARGS__);

} // namespace Mizu

#else

namespace Mizu
{

#define MIZU_ASSERT(cond, ...)

#define MIZU_VERIFY(cond, ...) \
    do                         \
    {                          \
        if (!(cond))           \
        {                      \
            exit(1);           \
        }                      \
    } while (false)

#define MIZU_UNREACHABLE(...) MIZU_VERIFY(false, __VA_ARGS__);

} // namespace Mizu

#endif