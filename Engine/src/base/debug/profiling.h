#pragma once

#if MIZU_DEBUG

#include <tracy/Tracy.hpp>

namespace Mizu
{

#define MIZU_PROFILE_SET_THREAD_NAME(_name) tracy::SetThreadName(_name)

#define MIZU_PROFILE_SCOPE ZoneScoped
#define MIZU_PROFILE_SCOPE_NAMED(_name) ZoneScopedN(_name)

#define MIZU_PROFILE_FRAME_MARK FrameMark

} // namespace Mizu

#else

namespace Mizu
{

#define MIZU_PROFILE_SET_THREAD_NAME(_name)

#define MIZU_PROFILE_SCOPE()
#define MIZU_PROFILE_SCOPE_NAMED(_name)

#define MIZU_PROFILE_FRAME_MARK

} // namespace Mizu

#endif