#include "mesh_cooker.h"

#include <glm/glm.hpp>

#include <assimp/scene.h>
#include <assimp/vector3.h>

#include "asset/asset_metadata.h"
#include "base/debug/assert.h"
#include "base/math/math.h"

namespace Mizu
{

//
// MeshCooker
//

bool MeshCooker::should_cook(const CookRequest&, const TimestampDb&) const
{
    // Filtering done by importer
    return true;
}

static glm::vec3 to_vec(const aiVector3D& vec)
{
    return {vec.x, vec.y, vec.z};
}

void MeshCooker::cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs)
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

    const bool has_normals = mesh->HasNormals();
    const bool has_uvs = mesh->HasTextureCoords(0);
    const bool has_tangents_and_bitangents = mesh->HasTangentsAndBitangents();

    if (!has_normals)
    {
        context.reporter.warning("Mesh '{}' has no normals, defaulting to (0, 0, 0)", mesh->mName.C_Str());
    }

    if (!has_uvs)
    {
        context.reporter.warning("Mesh '{}' has no UV channel 0, defaulting to (0, 0)", mesh->mName.C_Str());
    }

    if (!has_tangents_and_bitangents)
    {
        context.reporter.warning(
            "Mesh '{}' has no tangents and bitangents, defaulting to (0, 0, 0)", mesh->mName.C_Str());
    }

    for (uint32_t vertex_idx = 0; vertex_idx < mesh->mNumVertices; ++vertex_idx)
    {
        const glm::vec3 vertex = to_vec(mesh->mVertices[vertex_idx]);
        const glm::vec3 normal = has_normals ? to_vec(mesh->mNormals[vertex_idx]) : glm::vec3{0.0f};
        const glm::vec2 uv = has_uvs ? to_vec(mesh->mTextureCoords[0][vertex_idx]) : glm::vec2{0.0f};

        const glm::vec3 tangent = has_tangents_and_bitangents ? to_vec(mesh->mTangents[vertex_idx]) : glm::vec3{0.0f};
        const glm::vec3 bitangent =
            has_tangents_and_bitangents ? to_vec(mesh->mBitangents[vertex_idx]) : glm::vec3{0.0f};

        const float handedness = glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;

        vertices[vertex_idx] = MeshAssetVertex{
            .position = vertex,
            .normal = normal,
            .uv = {uv.x, 1.0f - uv.y},
            .tangent = glm::vec4(tangent, handedness),
        };
    }

    for (uint32_t face_idx = 0; face_idx < mesh->mNumFaces; ++face_idx)
    {
        const aiFace& face = mesh->mFaces[face_idx];
        if (face.mNumIndices != 3)
        {
            context.reporter.error(
                "Mesh '{}' has a face with {} indices, expected 3", mesh->mName.C_Str(), face.mNumIndices);
            return;
        }

        for (uint32_t index_idx = 0; index_idx < face.mNumIndices; ++index_idx)
        {
            indices[face_idx * 3 + index_idx] = face.mIndices[index_idx];
        }
    }

    const glm::vec3 aabb_min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
    const glm::vec3 aabb_max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};

    MeshAssetMetadata metadata{};
    metadata.vertex_count = mesh->mNumVertices;
    metadata.index_count = mesh->mNumFaces * 3u;
    metadata.index_format = IndexBufferFormat::UInt32;
    metadata.vertex_data_offset = 0;
    metadata.index_data_offset = math::align_up(
        metadata.vertex_data_offset + metadata.get_vertex_data_size_bytes(), metadata.get_index_element_size_bytes());
    metadata.bounding_box = math::AABB{aabb_min, aabb_max};

    const uint64_t total_size = TOTAL_MESH_METADATA_SIZE + metadata.get_total_size_bytes();

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Mesh");

    mesh_serialize_metadata(metadata, data);

    const uint64_t data_offset = TOTAL_MESH_METADATA_SIZE;

    const uint64_t vertex_offset = data_offset + metadata.vertex_data_offset;
    const uint64_t index_offset = data_offset + metadata.index_data_offset;

    memcpy(data.data() + vertex_offset, vertices.data(), metadata.get_vertex_data_size_bytes());
    memcpy(data.data() + index_offset, indices.data(), metadata.get_index_data_size_bytes());

    const std::string filename = std::to_string(get_mesh_asset_id(request.virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });
}

} // namespace Mizu
