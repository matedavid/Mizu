#pragma once

#include <cstdint>

#include "asset/asset_handle.h"

namespace Mizu
{

struct GpuMeshAllocationHandle
{
    MeshAssetHandle handle{};

    uint64_t vertex_offset = 0;
    uint64_t vertex_size = 0;

    uint64_t index_offset = 0;
    uint64_t index_size = 0;
};

struct GpuTextureAllocationHandle
{
    TextureAssetHandle handle{};
};

} // namespace Mizu