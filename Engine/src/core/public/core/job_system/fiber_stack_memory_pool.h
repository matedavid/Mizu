#pragma once

#include <cstdint>
#include <vector>

namespace Mizu
{

class FiberStackMemoryPool
{
  public:
    bool init(size_t num_pools, size_t size_bytes);

    uint8_t* get_memory(size_t index);

  private:
    size_t m_num_pools = 0;
    size_t m_size_bytes = 0;

    std::vector<uint8_t> m_pool;
};

} // namespace Mizu
