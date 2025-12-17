#include "render_core/rhi/rhi_helpers.h"

namespace Mizu
{

glm::uvec3 compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size)
{
    return {
        (thread_count.x + group_size.x - 1) / group_size.x,
        (thread_count.y + group_size.y - 1) / group_size.y,
        (thread_count.z + group_size.z - 1) / group_size.z};
}

} // namespace Mizu
