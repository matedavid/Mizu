#include "mesh_cooker.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <memory>

#include "asset/asset_metadata.h"
#include "base/debug/logging.h"
#include "base/utils/hash.h"

#include "material_cooker.h"

namespace Mizu
{

//
// MeshImporter
//

std::span<const std::string_view> MeshImporter::extensions() const
{
    static constexpr std::string_view extensions[]{
        ".gltf",
    };

    return extensions;
}

uint32_t MeshImporter::version() const
{
    return 0;
}

bool MeshImporter::should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const
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

void MeshImporter::import(const ImportRequest& request, const CookContext& context, std::vector<CookRequest>& outputs)
{
    constexpr uint32_t ASSIMP_IMPORT_FLAGS =
        aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;

    // Keep as shared_ptr because scenes and data created from it are deallocated with importer.
    std::shared_ptr<Assimp::Importer> importer = std::make_shared<Assimp::Importer>();

    const aiScene* scene = importer->ReadFile(request.path.string(), ASSIMP_IMPORT_FLAGS);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        MIZU_LOG_ERROR("Failed to import: {}, Assimp error: {}", request.path.string(), importer->GetErrorString());
        return;
    }

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

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        const MeshCookPayload mesh_payload{
            .importer = importer,
            .mesh = mesh,
        };

        outputs.push_back({
            .asset_type = AssetType::Mesh,
            .payload = mesh_payload,
        });
    }

    // Materials

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* material = scene->mMaterials[i];

        const MaterialCookPayload material_payload{
            .importer = importer,
            .material = material,

        };

        outputs.push_back({
            .asset_type = AssetType::Material,
            .payload = material_payload,
        });
    }

    // Prefab

    /* TODO:
    num_outputs += 1;

    outputs.push_back(
        ImportOutput{

            .asset_type = AssetType::Prefab,
        });
    */
}

//
// MeshCooker
//

bool MeshCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return false;
}

static uint64_t align_offset(uint64_t offset, uint64_t alignment)
{
    const uint64_t remainder = offset % alignment;
    if (remainder == 0)
        return offset;

    return offset + (alignment - remainder);
}

void MeshCooker::cook(const CookRequest& request, std::vector<SinkRequest>& outputs)
{
    const MeshCookPayload* payload = request.payload.get_if<MeshCookPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    const aiMesh* mesh = payload->mesh;

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
    std::vector<uint8_t> data(total_size);

    mesh_serialize_metadata(metadata, data);

    const size_t data_offset = METADATA_SHARED_INFO_SIZE + MESH_METADATA_SIZE;

    const size_t vertex_offset = data_offset + metadata.vertex_data_offset;
    const size_t index_offset = data_offset + metadata.index_data_offset;

    memcpy(data.data() + vertex_offset, vertices.data(), metadata.get_vertex_data_size_bytes());
    memcpy(data.data() + index_offset, indices.data(), metadata.get_index_data_size_bytes());

    // TODO: Create sink request
    (void)outputs;
}

} // namespace Mizu
