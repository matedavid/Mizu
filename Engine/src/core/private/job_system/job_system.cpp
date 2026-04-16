#include "core/job_system/job_system.h"

namespace Mizu
{

bool JobSystem2::init(uint32_t num_workers)
{
    (void)num_workers;
    return false;
}

void JobSystem2::worker_job() {}

} // namespace Mizu
