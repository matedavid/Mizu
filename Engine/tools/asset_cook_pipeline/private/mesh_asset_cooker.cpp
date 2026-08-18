#include "mesh_asset_cooker.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <glm/glm.hpp>

#include "asset/asset_metadata.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

std::span<std::string_view> MeshImporter::extensions() const
{
    static std::string_view extensions[]{
        ".gltf",
    };

    return extensions;
}

uint32_t MeshImporter::import(const ImportRequest& input, std::vector<CookRequest>& outputs) const
{
    constexpr uint32_t ASSIMP_IMPORT_FLAGS =
        aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;

    // Keep as shared_ptr because scenes and data created from it are deallocated with importer
    std::shared_ptr<Assimp::Importer> importer = std::make_shared<Assimp::Importer>();

    const aiScene* scene = importer->ReadFile(input.physical_path.string(), ASSIMP_IMPORT_FLAGS);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        MIZU_LOG_ERROR(
            "Failed to import mesh: {}. Assimp error: {}", input.physical_path.string(), importer->GetErrorString());
        return 0;
    }

    uint32_t num_outputs = 0;

    // Meshes

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        const MeshAssetCookInfo cook_info{
            .importer = importer,
            .mesh = mesh,
        };

        outputs.push_back({
            .asset_type = AssetCookType::Mesh,
            .cook_info = cook_info,
        });

        num_outputs += 1;
    }

    // Materials

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* material = scene->mMaterials[i];

        const MaterialAssetCookInfo cook_info{
            .importer = importer,
            .material = material,
        };

        outputs.push_back({
            .asset_type = AssetCookType::Material,
            .cook_info = cook_info,
        });

        num_outputs += 1;
    }

    // Prefab

    /* TODO:
    num_outputs += 1;

    outputs.push_back(
        ImportOutput{
            .asset_type = AssetType::Prefab,
        });
    */

    return num_outputs;
}

static uint64_t align_offset(uint64_t offset, uint64_t alignment)
{
    const uint64_t remainder = offset % alignment;
    if (remainder == 0)
        return offset;

    return offset + (alignment - remainder);
}

bool MeshCooker::cook(const AssetCookInfoT& cook_info) const
{
    const MeshAssetCookInfo* info = std::get_if<MeshAssetCookInfo>(&cook_info);
    if (info == nullptr)
    {
        MIZU_ASSERT(false, "Invalid AssetCookInfoT, should be MeshAssetCookInfo");
        return false;
    }

    const aiMesh* mesh = info->mesh;

    std::vector<MeshAssetVertex> vertices(mesh->mNumVertices);
    std::vector<uint32_t> indices(mesh->mNumFaces * 3);

    for (uint32_t vertex_idx = 0; vertex_idx < mesh->mNumVertices; ++vertex_idx)
    {
        const aiVector3D& vertex = mesh->mVertices[vertex_idx];
        const aiVector3D& normal = mesh->mNormals[vertex_idx];
        const aiVector3D& uv = mesh->mTextureCoords[0][vertex_idx];

        vertices[vertex_idx] = MeshAssetVertex{
            .position = {vertex.x, vertex.y, vertex.z},
            .normal = {normal.x, normal.y, normal.z},
            .uv = {uv.x, 1.0f - uv.y},
        };
    }

    for (uint32_t face_idx = 0; face_idx < mesh->mNumFaces; ++face_idx)
    {
        const aiFace& face = mesh->mFaces[face_idx];
        MIZU_ASSERT(face.mNumIndices == 3, "Mesh is expected to be triangulated");

        for (uint32_t index_idx = 0; index_idx < face.mNumIndices; ++index_idx)
        {
            indices[face_idx * 3 + index_idx] = face.mIndices[index_idx];
        }
    }

    const glm::vec3 aabb_min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
    const glm::vec3 aabb_max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};

    MeshMetadata metadata{};
    metadata.vertex_count = mesh->mNumVertices;
    metadata.index_count = mesh->mNumFaces * 3u;
    metadata.index_format = IndexBufferFormat::UInt32;
    metadata.vertex_data_offset = 0;
    metadata.index_data_offset = align_offset(
        metadata.vertex_data_offset + metadata.get_vertex_data_size_bytes(), metadata.get_index_element_size_bytes());
    metadata.bounding_box = AABB{aabb_min, aabb_max};

    const size_t total_size = METADATA_SHARED_INFO_SIZE + MESH_METADATA_SIZE + metadata.get_total_size_bytes();
    std::vector<uint8_t> payload(total_size);

    mesh_serialize_metadata(metadata, payload);

    const size_t data_offset = METADATA_SHARED_INFO_SIZE + MESH_METADATA_SIZE;

    const size_t vertex_offset = data_offset + metadata.vertex_data_offset;
    const size_t index_offset = data_offset + metadata.index_data_offset;

    memcpy(payload.data() + vertex_offset, vertices.data(), metadata.get_vertex_data_size_bytes());
    memcpy(payload.data() + index_offset, indices.data(), metadata.get_index_data_size_bytes());

    // TODO: Write payload to disk somehow :)

    return true;
}

} // namespace Mizu