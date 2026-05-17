#pragma once

#include <cstdint>

namespace Mizu
{

struct GpuAllocationHandleBase
{
};

struct GpuMeshAllocationHandle
{
};

struct GpuTextureAllocationHandle
{
};

class GpuMeshPool
{
  public:
    GpuMeshPool() = default;

    bool init(uint64_t size);
};

class GpuTexturePool
{
  public:
    GpuTexturePool() = default;

    bool init(uint64_t size);
};

} // namespace Mizu
