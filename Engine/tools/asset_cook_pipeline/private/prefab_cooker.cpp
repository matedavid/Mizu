#include "prefab_cooker.h"

#include <utility>

#include "asset/asset_metadata.h"
#include "base/debug/assert.h"

namespace Mizu
{

bool PrefabCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
}

void PrefabCooker::cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs)
{
    const PrefabCookPayload* payload = request.payload.get_if<PrefabCookPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    const PrefabMetadata metadata{
        .num_meshes = static_cast<uint32_t>(payload->mesh_info.size()),
    };

    const size_t total_size =
        TOTAL_PREFAB_METADATA_SIZE + metadata.num_meshes * sizeof(std::declval<MeshAssetHandle>().get_id()) * 2;

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Prefab");

    prefab_serialize_metadata(metadata, data);

    size_t data_offset = TOTAL_PREFAB_METADATA_SIZE;

    for (const PrefabMeshInfo& mesh_info : payload->mesh_info)
    {
        const uint64_t mesh_id = mesh_info.mesh_handle.get_id();
        const uint64_t material_id = mesh_info.material_handle.get_id();

        memcpy(data.data() + data_offset, &mesh_id, sizeof(mesh_id));
        data_offset += sizeof(mesh_id);

        memcpy(data.data() + data_offset, &material_id, sizeof(material_id));
        data_offset += sizeof(material_id);
    }

    const std::string filename = std::to_string(hash_compute(request.virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });
}

} // namespace Mizu