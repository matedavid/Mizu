#pragma once

#ifndef NDEBUG

#include "utility/logging.h"
#include <cassert>

namespace Mizu {

// Only fails on Debug
#define MIZU_ASSERT(cond, ...)           \
    do {                                 \
        if (!(cond)) {                   \
            MIZU_LOG_ERROR(__VA_ARGS__); \
            exit(1);                     \
        }                                \
    } while (false)

// Fails also on release
#define MIZU_VERIFY(cond, ...) MIZU_ASSERT(cond, __VA_ARGS__)

} // namespace Mizu

#else

namespace Mizu {

#define MIZU_ASSERT(cond, ...)

#define MIZU_VERIFY(cond, ...) \
    do {                       \
        if (!(cond)) {         \
            exit(1);           \
        }                      \
    } while (false)

} // namespace Mizu

#endif