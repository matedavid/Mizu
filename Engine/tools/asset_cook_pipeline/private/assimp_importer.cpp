#include "assimp_importer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <format>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/utils/hash.h"

#include "material_cooker.h"
#include "mesh_cooker.h"
#include "prefab_cooker.h"

namespace Mizu
{

std::span<const std::string_view> AssimpImporter::extensions() const
{
    static constexpr std::string_view extensions[]{
        ".gltf",
    };

    return extensions;
}

uint32_t AssimpImporter::version() const
{
    return 0;
}

bool AssimpImporter::should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const
{
    const size_t id = hash_compute(request.virtual_path);

    const uint64_t last_write_time =
        static_cast<uint64_t>(std::filesystem::last_write_time(request.path).time_since_epoch().count());

    const Timestamp ts{
        .ts = last_write_time,
        .version = version(),
    };

    return timestamp_db.is_different(id, ts);
}

static std::string get_subasset_virtual_path(
    std::string_view asset_virtual_path,
    std::string_view subasset_name,
    std::string_view default_name)
{
    return std::format("{}#{}", asset_virtual_path, !subasset_name.empty() ? subasset_name : default_name);
}

void AssimpImporter::import(const ImportRequest& request, const CookContext& context, std::vector<CookRequest>& outputs)
{
    constexpr uint32_t ASSIMP_IMPORT_FLAGS = aiProcess_Triangulate | aiProcess_CalcTangentSpace
                                             | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph
                                             | (uint32_t)aiProcess_GenBoundingBoxes;

    // Keep as shared_ptr because scenes and data created from it are deallocated with importer.
    std::shared_ptr<Assimp::Importer> importer = std::make_shared<Assimp::Importer>();

    const aiScene* scene = importer->ReadFile(request.path.string(), ASSIMP_IMPORT_FLAGS);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        MIZU_LOG_ERROR("Failed to import: {}, Assimp error: {}", request.path.string(), importer->GetErrorString());
        return;
    }

    // TODO: Probably not best place
    {
        const size_t id = hash_compute(request.virtual_path);

        const uint64_t last_write_time =
            static_cast<uint64_t>(std::filesystem::last_write_time(request.path).time_since_epoch().count());

        const Timestamp ts{
            .ts = last_write_time,
            .version = version(),
        };

        context.timestamp_db.record(id, ts);
    }

    // Meshes

    std::vector<MeshAssetHandle> mesh_handles(scene->mNumMeshes);

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        const std::string default_name = std::format("mesh_{}", i);
        const std::string virtual_path =
            get_subasset_virtual_path(request.virtual_path, mesh->mName.C_Str(), default_name);

        const MeshCookPayload mesh_payload{
            .importer = importer,
            .mesh = mesh,
        };

        outputs.push_back({
            .asset_type = AssetType::Mesh,
            .virtual_path = virtual_path,
            .payload = mesh_payload,
        });

        mesh_handles[i] = MeshAssetHandle{get_mesh_asset_id(virtual_path)};
    }

    // Materials

    std::vector<MaterialAssetHandle> material_handles(scene->mNumMaterials);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* material = scene->mMaterials[i];

        const std::string default_name = std::format("material_{}", i);
        const std::string virtual_path =
            get_subasset_virtual_path(request.virtual_path, material->GetName().C_Str(), default_name);

        const MaterialCookPayload material_payload{
            .importer = importer,
            .material = material,
            .parent_path = request.path.parent_path(),
        };

        outputs.push_back({
            .asset_type = AssetType::Material,
            .virtual_path = virtual_path,
            .payload = material_payload,
        });

        material_handles[i] = MaterialAssetHandle{get_material_asset_id(virtual_path)};
    }

    // Prefab

    std::vector<PrefabMeshInfo> prefab_mesh_info{};

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        MIZU_ASSERT(i < mesh_handles.size(), "Invalid MeshIndex {}, max is {}", i, mesh_handles.size());
        MIZU_ASSERT(
            mesh->mMaterialIndex < material_handles.size(),
            "Invalid MaterialIndex {}, max is {}",
            mesh->mMaterialIndex,
            material_handles.size());

        const PrefabMeshInfo mesh_info{
            .mesh_handle = mesh_handles[i],
            .material_handle = material_handles[mesh->mMaterialIndex],
        };

        prefab_mesh_info.push_back(mesh_info);
    }

    outputs.push_back({
        .asset_type = AssetType::Prefab,
        .virtual_path = request.virtual_path,
        .payload =
            PrefabCookPayload{
                .mesh_info = prefab_mesh_info,
            },
    });
}

} // namespace Mizu