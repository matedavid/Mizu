#pragma once

#if MIZU_LOGGING_ENABLED

#include <spdlog/spdlog.h>

namespace Mizu
{

#define MIZU_LOG_SETUP spdlog::set_pattern("[%^%l%$] [%s:%#] %v")

#define MIZU_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define MIZU_LOG_WARNING(...) SPDLOG_WARN(__VA_ARGS__)
#define MIZU_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)

#define MIZU_LOG_ONCE(_log_func, ...)             \
    do                                            \
    {                                             \
        static bool s_logged__##__LINE__ = false; \
        if (!s_logged__##__LINE__)                \
        {                                         \
            s_logged__##__LINE__ = true;          \
            _log_func(__VA_ARGS__);               \
        }                                         \
    } while (0)

#define MIZU_LOG_ONCE_INFO(...) MIZU_LOG_ONCE(MIZU_LOG_INFO, __VA_ARGS__)
#define MIZU_LOG_ONCE_WARNING(...) MIZU_LOG_ONCE(MIZU_LOG_WARNING, __VA_ARGS__)
#define MIZU_LOG_ONCE_ERROR(...) MIZU_LOG_ONCE(MIZU_LOG_ERROR, __VA_ARGS__)

} // namespace Mizu

#else

#define MIZU_LOG_DISCARD(...) (void)sizeof(__VA_ARGS__)

#define MIZU_LOG_SETUP

#define MIZU_LOG_INFO(...) MIZU_LOG_DISCARD(__VA_ARGS__)
#define MIZU_LOG_WARNING(...) MIZU_LOG_DISCARD(__VA_ARGS__)
#define MIZU_LOG_ERROR(...) MIZU_LOG_DISCARD(__VA_ARGS__)

#define MIZU_LOG_ONCE_INFO(...) MIZU_LOG_DISCARD(__VA_ARGS__)
#define MIZU_LOG_ONCE_WARNING(...) MIZU_LOG_DISCARD(__VA_ARGS__)
#define MIZU_LOG_ONCE_ERROR(...) MIZU_LOG_DISCARD(__VA_ARGS__)

#endif
