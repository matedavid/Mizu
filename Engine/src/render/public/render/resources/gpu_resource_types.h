#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "asset/asset_handle.h"
#include "asset/asset_loader.h"

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

struct GpuMeshResidentRecord
{
    GpuMeshAllocationHandle allocation{};
    MeshPayload payload{};
};

struct GpuMeshDrawPayload
{
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    uint32_t first_vertex = 0;
    uint32_t first_index = 0;
};

struct GpuTextureAllocationHandle
{
    TextureAssetHandle handle{};
};

struct GpuTextureResidentRecord
{
    GpuTextureAllocationHandle allocation{};
    TexturePayload payload{};
};

struct TransformInfo
{
    glm::mat4 transform;
    glm::mat4 normal_matrix;
};

} // namespace Mizu