#include "material_cooker.h"

#include <assimp/scene.h>
#include <optional>

#include "asset/asset_metadata.h"

namespace Mizu
{

bool MaterialCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
}

static std::optional<AssetMount> get_asset_mount(const AssetMountTable& table, const std::filesystem::path& path)
{
    for (const AssetMount& mount : table.get_asset_mounts())
    {
        const std::filesystem::path relative = std::filesystem::relative(path, mount.path);

        if (!relative.empty())
        {
            return mount;
        }
    }

    return std::nullopt;
}

void MaterialCooker::cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs)
{
    const MaterialCookPayload* payload = request.payload.get_if<MaterialCookPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    const aiMaterial* material = payload->material;

    const auto get_material_texture_path =
        [](const aiMaterial& material, aiTextureType type, uint32_t index, aiString& texture_path) {
            return material.GetTexture(type, index, &texture_path) == aiReturn_SUCCESS;
        };

    std::unordered_set<uint64_t> unique_texture_ids{};

    const auto add_texture_dependency = [&](const aiString& texture_name, MaterialAssetMetadata& metadata) {
        const std::filesystem::path texture_path = payload->parent_path / texture_name.C_Str();

        if (!std::filesystem::exists(texture_path))
        {
            MIZU_LOG_ERROR("Texture path: {} does not exist", texture_path.string());
            return false;
        }

        const std::optional<AssetMount> mount = get_asset_mount(context.asset_mounts, texture_path);
        if (!mount.has_value())
            return false;

        const std::string virtual_path =
            std::format("{}:{}", mount->name, std::filesystem::relative(texture_path, mount->path).string());

        const AssetHandleId id = get_texture_asset_id(virtual_path);

        if (unique_texture_ids.insert(id).second)
            metadata.texture_handles.push_back(TextureAssetHandle{id});

        return true;
    };

    MaterialAssetMetadata metadata{};
    metadata.num_textures = 0;

    aiString texture_path{};
    if (get_material_texture_path(*material, aiTextureType_BASE_COLOR, 0, texture_path)
        && !add_texture_dependency(texture_path, metadata))
    {
        return;
    }

    if (get_material_texture_path(*material, aiTextureType_METALNESS, 0, texture_path)
        && !add_texture_dependency(texture_path, metadata))
    {
        return;
    }

    if (get_material_texture_path(*material, aiTextureType_DIFFUSE_ROUGHNESS, 0, texture_path)
        && !add_texture_dependency(texture_path, metadata))
    {
        return;
    }

    if (get_material_texture_path(*material, aiTextureType_LIGHTMAP, 0, texture_path)
        && !add_texture_dependency(texture_path, metadata))
    {
        return;
    }

    metadata.num_textures = static_cast<uint32_t>(metadata.texture_handles.size());

    const size_t total_size = TOTAL_MATERIAL_METADATA_SIZE;

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Prefab");

    material_serialize_metadata(metadata, data);

    const std::string filename = std::to_string(get_material_asset_id(request.virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });
}

} // namespace Mizu
