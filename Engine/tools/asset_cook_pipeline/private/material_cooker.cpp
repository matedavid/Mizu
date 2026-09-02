#include "material_cooker.h"

#include <assimp/scene.h>

#include "asset/asset_metadata.h"

#include "asset_cook_pipeline_helpers.h"

namespace Mizu
{

bool MaterialCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
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

    aiString texture_path{};

    const auto load_texture = [&](aiTextureType type) -> TextureAssetHandle {
        if (get_material_texture_path(*material, type, 0, texture_path))
        {
            const std::filesystem::path texture_path_fs = payload->parent_path / texture_path.C_Str();
            if (std::filesystem::exists(texture_path_fs))
            {
                const std::filesystem::path relative_path =
                    std::filesystem::relative(texture_path_fs, request.asset_mount.path);
                const std::string virtual_path = create_virtual_path(relative_path, request.asset_mount);

                return TextureAssetHandle{get_texture_asset_id(virtual_path)};
            }
        }

        // TODO: HACK consider texture handle == DEFAULT_TEXTURE_HANDLE_ID as the default texture
        // DEFAULT_TEXTURE_HANDLE_ID is defined in residency_system.cpp
        return TextureAssetHandle{0};
    };

    const TextureAssetHandle albedo_handle = load_texture(aiTextureType_BASE_COLOR);
    const TextureAssetHandle metallic_handle = load_texture(aiTextureType_METALNESS);
    const TextureAssetHandle roughness_handle = load_texture(aiTextureType_DIFFUSE_ROUGHNESS);
    const TextureAssetHandle ao_handle = load_texture(aiTextureType_LIGHTMAP);

    MaterialAssetMetadata metadata{};
    metadata.num_textures = 4;

    metadata.texture_handles.push_back(albedo_handle);
    metadata.texture_handles.push_back(metallic_handle);
    metadata.texture_handles.push_back(roughness_handle);
    metadata.texture_handles.push_back(ao_handle);

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
