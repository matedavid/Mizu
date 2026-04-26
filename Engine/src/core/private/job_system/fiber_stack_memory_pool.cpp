#include "core/job_system/fiber_stack_memory_pool.h"

#include "base/debug/assert.h"

namespace Mizu
{

bool FiberStackMemoryPool::init(size_t num_pools, size_t size_bytes)
{
    m_num_pools = num_pools;
    m_size_bytes = size_bytes;

    const size_t total_size = num_pools * size_bytes;
    m_pool = std::vector<uint8_t>(total_size);

    return true;
}

uint8_t* FiberStackMemoryPool::get_memory(size_t index)
{
    MIZU_ASSERT(index < m_num_pools, "Trying to get memory with invalid index");

    const size_t pool_index = index * m_size_bytes;
    MIZU_ASSERT(pool_index < m_pool.size() && pool_index + m_size_bytes < m_pool.size(), "Invalid index");

    return &m_pool[pool_index];
}

} // namespace Mizu
