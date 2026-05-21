#include "asset/dev_asset_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <filesystem>

#include "base/debug/assert.h"

namespace Mizu
{

static constexpr uint32_t AssimpImportFlags =
    aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;

static const aiScene* load_scene(const char* path, Assimp::Importer& importer)
{
    const aiScene* scene = importer.ReadFile(path, AssimpImportFlags);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        return nullptr;

    return scene;
}

static uint64_t align_offset(uint64_t offset, uint64_t alignment)
{
    const uint64_t remainder = offset % alignment;
    if (remainder == 0)
        return offset;

    return offset + (alignment - remainder);
}

DevAssetLoader::DevAssetLoader(const AssetRegistry& registry) : m_registry(registry) {}

std::optional<MeshAssetRecord> DevAssetLoader::get_mesh_record(const MeshAssetHandle& handle)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    MIZU_ASSERT(
        std::filesystem::exists(location.physical_path),
        "Mesh asset path: {} does not exist",
        location.physical_path.string());

    Assimp::Importer importer;

    const std::string path_str = location.physical_path.string();
    const aiScene* scene = load_scene(path_str.c_str(), importer);
    if (scene == nullptr)
        return std::nullopt;

    // TODO: not correct
    const aiMesh* mesh = scene->mMeshes[0];

    MeshPayloadLayout payload{};
    payload.vertex_count = mesh->mNumVertices;
    payload.index_count = mesh->mNumFaces * 3;
    payload.index_format = IndexBufferFormat::UInt32;
    payload.vertex_data_offset = 0;
    payload.index_data_offset = align_offset(
        payload.vertex_data_offset + payload.get_vertex_data_size_bytes(), payload.get_index_data_alignment_bytes());
    payload.size_bytes = payload.index_data_offset + payload.get_index_data_size_bytes();
    payload.alignment = 1;

    MeshAssetRecord record{};
    record.handle = handle;
    record.payload = payload;

    return record;
}

std::optional<TextureAssetRecord> DevAssetLoader::get_texture_record(const TextureAssetHandle& handle)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    (void)location;

    return TextureAssetRecord{};
}

bool DevAssetLoader::load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    MIZU_ASSERT(
        std::filesystem::exists(location.physical_path),
        "Mesh asset path: {} does not exist",
        location.physical_path.string());

    const std::optional<MeshAssetRecord> record = get_mesh_record(handle);
    if (!record.has_value())
        return false;

    MIZU_ASSERT(
        destination.size() >= record->payload.size_bytes,
        "Destination buffer size: {} is smaller than the required size: {}",
        destination.size(),
        record->payload.size_bytes);

    Assimp::Importer importer;

    const std::string path_str = location.physical_path.string();
    const aiScene* scene = load_scene(path_str.c_str(), importer);
    if (scene == nullptr)
        return false;

    const aiMesh* mesh = scene->mMeshes[0];

    for (uint32_t vertex_idx = 0; vertex_idx < mesh->mNumVertices; ++vertex_idx)
    {
        const aiVector3D& vertex = mesh->mVertices[vertex_idx];
        const aiVector3D& normal = mesh->mNormals[vertex_idx];
        const aiVector3D& uv = mesh->mTextureCoords[0][vertex_idx];

        MeshAssetVertex asset_vertex{};
        asset_vertex.position = {vertex.x, vertex.y, vertex.z};
        asset_vertex.normal = {normal.x, normal.y, normal.z};
        asset_vertex.uv = {uv.x, 1.0f - uv.y};

        const size_t destination_idx = record->payload.vertex_data_offset + vertex_idx * sizeof(MeshAssetVertex);
        memcpy(destination.data() + destination_idx, &asset_vertex, sizeof(MeshAssetVertex));
    }

    for (uint32_t face_idx = 0; face_idx < mesh->mNumFaces; ++face_idx)
    {
        const aiFace& face = mesh->mFaces[face_idx];
        MIZU_ASSERT(face.mNumIndices == 3, "Mesh is expected to be triangulated");

        for (uint32_t idx = 0; idx < face.mNumIndices; ++idx)
        {
            const uint32_t index = face.mIndices[idx];
            const size_t destination_idx = record->payload.index_data_offset + (face_idx * 3 + idx) * sizeof(uint32_t);
            memcpy(destination.data() + destination_idx, &index, sizeof(uint32_t));
        }
    }

    return true;
}

bool DevAssetLoader::load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    (void)location;
    (void)destination;

    return false;
}

} // namespace Mizu