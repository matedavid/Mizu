#pragma once

#ifdef MIZU_DEBUG

#include <spdlog/spdlog.h>

namespace Mizu {

#define MIZU_LOG_SETUP spdlog::set_pattern("[%^%l%$] [%s:%#] %v")

#define MIZU_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define MIZU_LOG_WARNING(...) SPDLOG_WARN(__VA_ARGS__)
#define MIZU_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)

} // namespace Mizu

#else

namespace Mizu {

#define MIZU_LOG_SETUP

#define MIZU_LOG_INFO(...)
#define MIZU_LOG_WARNING(...)
#define MIZU_LOG_ERROR(...)

} // namespace Mizu

#endif
