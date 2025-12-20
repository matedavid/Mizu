#include "render_core/rhi/buffer_resource.h"

namespace Mizu
{

#if MIZU_DEBUG

std::string_view buffer_resource_state_to_string(BufferResourceState state)
{
    switch (state)
    {
    case BufferResourceState::Undefined:
        return "Undefined";
    case BufferResourceState::UnorderedAccess:
        return "UnorderedAccess";
    case BufferResourceState::TransferSrc:
        return "TransferSrc";
    case BufferResourceState::TransferDst:
        return "TransferDst";
    case BufferResourceState::ShaderReadOnly:
        return "ShaderReadOnly";
    }
}

#endif

} // namespace Mizu