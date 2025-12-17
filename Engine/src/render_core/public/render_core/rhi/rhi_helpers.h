#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "mizu_render_core_module.h"

namespace Mizu
{

MIZU_RENDER_CORE_API glm::uvec3 compute_group_count(glm::uvec3 thread_count, glm::uvec3 group_size);

} // namespace Mizu
