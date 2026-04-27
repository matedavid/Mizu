#pragma once

#if MIZU_DEBUG

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

namespace Mizu
{

#define MIZU_PROFILE_SET_THREAD_NAME(_name) tracy::SetThreadName(_name)

#define MIZU_PROFILE_SCOPED ZoneScoped
#define MIZU_PROFILE_SCOPED_NAME(_name) ZoneScopedN(_name)

#define MIZU_PROFILE_ZONE_BEGIN(_ctx) TracyCZone(_ctx, true)
#define MIZU_PROFILE_ZONE_BEGIN_NAME(_ctx, _name) TracyCZoneN(_ctx, _name, true)

#define MIZU_PROFILE_ZONE_END(_ctx) TracyCZoneEnd(_ctx)

#define MIZU_PROFILE_FRAME_MARK FrameMark

#define MIZU_PROFILE_FIBER_ENTER(_name) TracyCFiberEnter(_name)
#define MIZU_PROFILE_FIBER_LEAVE TracyCFiberLeave

} // namespace Mizu

#else

namespace Mizu
{

#define MIZU_PROFILE_SET_THREAD_NAME(_name)

#define MIZU_PROFILE_SCOPED
#define MIZU_PROFILE_SCOPED_NAME(_name)

#define MIZU_PROFILE_ZONE_BEGIN(_ctx)
#define MIZU_PROFILE_ZONE_BEGIN_NAME(_ctx, _name)

#define MIZU_PROFILE_ZONE_END(_ctx)

#define MIZU_PROFILE_FRAME_MARK

#define MIZU_PROFILE_FIBER_ENTER(_name)
#define MIZU_PROFILE_FIBER_LEAVE

} // namespace Mizu

#endif
