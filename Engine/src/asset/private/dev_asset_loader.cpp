#include "asset/dev_asset_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <cstring>
#include <filesystem>
#include <stb_image.h>
#include <string>
#include <unordered_set>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

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

    const SpecificMeshAssetInfo* specific_info = std::get_if<SpecificMeshAssetInfo>(&location.specific_info);
    MIZU_ASSERT(
        specific_info != nullptr, "Mesh asset handle: {} does not contain specific mesh asset info", handle.get_id());

    MIZU_ASSERT(
        specific_info->submesh < scene->mNumMeshes,
        "Mesh asset handle: {} submesh index: {} is out of bounds for mesh with {} submeshes",
        handle.get_id(),
        specific_info->submesh,
        scene->mNumMeshes);

    const aiMesh* mesh = scene->mMeshes[specific_info->submesh];

    MeshPayload payload{};
    payload.vertex_count = mesh->mNumVertices;
    payload.index_count = mesh->mNumFaces * 3;
    payload.index_format = IndexBufferFormat::UInt32;
    payload.vertex_data_offset = 0;
    payload.index_data_offset = align_offset(
        payload.vertex_data_offset + payload.get_vertex_data_size_bytes(), payload.get_index_element_size_bytes());

    MeshAssetRecord record{};
    record.handle = handle;
    record.payload = payload;

    return record;
}

std::optional<TextureAssetRecord> DevAssetLoader::get_texture_record(const TextureAssetHandle& handle)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);

    MIZU_ASSERT(
        std::filesystem::exists(location.physical_path),
        "Texture asset path: {} does not exist",
        location.physical_path.string());

    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info(location.physical_path.string().c_str(), &width, &height, &channels) == 0)
        return std::nullopt;

    TextureAssetRecord record{};
    record.handle = handle;
    record.payload.width = static_cast<uint32_t>(width);
    record.payload.height = static_cast<uint32_t>(height);
    record.payload.depth = 1;
    record.payload.num_mips = 1;
    record.payload.format = ImageFormat::R8G8B8A8_UNORM;

    return record;
}

static bool get_material_texture_path(
    const aiMaterial& material,
    aiTextureType type,
    uint32_t index,
    aiString& texture_path)
{
    return material.GetTexture(type, index, &texture_path) == aiReturn_SUCCESS;
}

std::optional<MaterialAssetRecord> DevAssetLoader::get_material_record(const MaterialAssetHandle& handle)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    MIZU_ASSERT(
        std::filesystem::exists(location.physical_path),
        "Material asset path: {} does not exist",
        location.physical_path.string());

    Assimp::Importer importer;

    const std::string path_str = location.physical_path.string();
    const aiScene* scene = load_scene(path_str.c_str(), importer);
    if (scene == nullptr)
        return std::nullopt;

    const SpecificMaterialAssetInfo* specific_info = std::get_if<SpecificMaterialAssetInfo>(&location.specific_info);
    MIZU_ASSERT(
        specific_info != nullptr,
        "Material asset handle: {} does not contain specific material asset info",
        handle.get_id());

    MIZU_ASSERT(
        specific_info->mesh_material < scene->mNumMeshes,
        "Material asset handle: {} mesh material index: {} is out of bounds for mesh with {} submeshes",
        handle.get_id(),
        specific_info->mesh_material,
        scene->mNumMeshes);

    const aiMesh* mesh = scene->mMeshes[specific_info->mesh_material];
    MIZU_ASSERT(
        mesh->mMaterialIndex < scene->mNumMaterials,
        "Mesh material index: {} references invalid material index: {} for scene with {} materials",
        specific_info->mesh_material,
        mesh->mMaterialIndex,
        scene->mNumMaterials);

    const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    MaterialAssetRecord record{};
    record.handle = handle;

    std::unordered_set<uint64_t> unique_texture_ids{};

    const auto add_texture_dependency = [&](const aiString& texture_name) -> bool {
        const std::filesystem::path texture_path = location.physical_path.parent_path() / texture_name.C_Str();
        MIZU_ASSERT(std::filesystem::exists(texture_path), "Texture path: {} does not exist", texture_path.string());

        const TextureAssetHandle texture_handle = m_registry.get_texture_handle_from_physical_path(texture_path);
        if (!texture_handle.is_valid())
        {
            MIZU_LOG_ERROR(
                "Failed to create texture handle for material asset: {} from texture path: {}",
                handle.get_id(),
                texture_path.string());
            return false;
        }

        if (unique_texture_ids.insert(texture_handle.get_id()).second)
            record.texture_handles.push_back(texture_handle);

        return true;
    };

    aiString texture_path{};
    if (get_material_texture_path(*material, aiTextureType_BASE_COLOR, 0, texture_path)
        && !add_texture_dependency(texture_path))
    {
        return std::nullopt;
    }

    if (get_material_texture_path(*material, aiTextureType_METALNESS, 0, texture_path)
        && !add_texture_dependency(texture_path))
    {
        return std::nullopt;
    }

    if (get_material_texture_path(*material, aiTextureType_DIFFUSE_ROUGHNESS, 0, texture_path)
        && !add_texture_dependency(texture_path))
    {
        return std::nullopt;
    }

    if (get_material_texture_path(*material, aiTextureType_LIGHTMAP, 0, texture_path)
        && !add_texture_dependency(texture_path))
    {
        return std::nullopt;
    }

    return record;
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
        destination.size() >= record->payload.get_total_size_bytes(),
        "Destination buffer size: {} is smaller than the required size: {}",
        destination.size(),
        record->payload.get_total_size_bytes());

    Assimp::Importer importer;

    const std::string path_str = location.physical_path.string();
    const aiScene* scene = load_scene(path_str.c_str(), importer);
    if (scene == nullptr)
        return false;

    const SpecificMeshAssetInfo* specific_info = std::get_if<SpecificMeshAssetInfo>(&location.specific_info);
    MIZU_ASSERT(
        specific_info != nullptr, "Mesh asset handle: {} does not contain specific mesh asset info", handle.get_id());

    MIZU_ASSERT(
        specific_info->submesh < scene->mNumMeshes,
        "Mesh asset handle: {} submesh index: {} is out of bounds for mesh with {} submeshes",
        handle.get_id(),
        specific_info->submesh,
        scene->mNumMeshes);

    const aiMesh* mesh = scene->mMeshes[specific_info->submesh];

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

    MIZU_ASSERT(
        std::filesystem::exists(location.physical_path),
        "Texture asset path: {} does not exist",
        location.physical_path.string());

    const std::optional<TextureAssetRecord> record = get_texture_record(handle);
    if (!record.has_value())
        return false;

    MIZU_ASSERT(
        destination.size() >= record->payload.get_total_size_bytes(),
        "Destination buffer size: {} is smaller than the required size: {}",
        destination.size(),
        record->payload.get_total_size_bytes());

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(location.physical_path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
        return false;

    const uint64_t size_bytes = record->payload.get_total_size_bytes();
    memcpy(destination.data(), pixels, size_bytes);
    stbi_image_free(pixels);

    return true;
}

} // namespace Mizu