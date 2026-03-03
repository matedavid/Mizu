#include "render_core/rhi/acceleration_structure.h"

namespace Mizu
{

#if MIZU_DEBUG

std::string_view acceleration_structure_resource_state_to_string(AccelerationStructureResourceState state)
{
    switch (state)
    {
    case AccelerationStructureResourceState::Undefined:
        return "Undefined";
    case AccelerationStructureResourceState::AccelStructRead:
        return "AccelStructRead";
    case AccelerationStructureResourceState::AccelStructWrite:
        return "AccelStructWrite";
    }
}

#endif
} // namespace Mizu