#include "mesh_cooker.h"

#include <array>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <meshoptimizer.h>

#include "asset/asset_metadata.h"
#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/math/shapes.h"

namespace Mizu
{

namespace
{

glm::vec3 to_vec(const aiVector3D& vec)
{
    return {vec.x, vec.y, vec.z};
}

float compute_texel_factor(std::span<const MeshAssetVertex> vertices, std::span<const uint32_t> indices)
{
    float world_area_sum = 0.0f;
    float uv_area_sum = 0.0f;

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const MeshAssetVertex& a = vertices[indices[i + 0]];
        const MeshAssetVertex& b = vertices[indices[i + 1]];
        const MeshAssetVertex& c = vertices[indices[i + 2]];

        const float world_area = 0.5f * glm::length(glm::cross(b.position - a.position, c.position - a.position));

        const glm::vec2 e0 = b.uv - a.uv;
        const glm::vec2 e1 = c.uv - a.uv;
        const float uv_area = 0.5f * std::abs(e0.x * e1.y - e0.y * e1.x);

        if (world_area < glm::epsilon<float>() || uv_area < glm::epsilon<float>())
            continue;

        world_area_sum += world_area;
        uv_area_sum += uv_area;
    }

    if (uv_area_sum <= 0.0)
        return 0.0f;

    return std::sqrt(world_area_sum / uv_area_sum);
}

constexpr bool GENERATE_LODS = true;
constexpr uint8_t NUM_LODS = 6;
constexpr uint8_t NUM_STREAMABLE_LODS = 2;

constexpr uint8_t LODS_TO_GENERATE = GENERATE_LODS ? NUM_LODS : 1;

// Percentage to reduce triangles in each lod, so lod1_num_triangles = LOD_REDUCTION * lod0_num_triangles
constexpr float LOD_REDUCTION = 0.5f;
// Min error growth between consecutive lods, a lod that doesn't reach it replaces the previous one
constexpr float MIN_LOD_ERROR_GROWTH = 1.5f;
// Max simplification error allowed on screen, as a fraction of the screen height
constexpr float LOD_MAX_SCREEN_ERROR = 1.0f / (1080.0f / 2.0f);

//                                  { normal.x normal.y normal.z uv.x uv.y }
constexpr float LOD_ATTRIBUTE_WEIGHTS[] = {0.5f, 0.5f, 0.5f, 1.0f, 1.0f};

struct LodIndexRange
{
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    float error = 0.0f;
};

using LodIndexRanges = inplace_vector<LodIndexRange, MAX_NUM_MESH_LODS>;

struct LodVertexRemap
{
    std::vector<uint32_t> remap;
    std::array<uint32_t, MAX_NUM_MESH_LODS> lod_vertex_counts{};
};

void merge_duplicate_vertices(std::vector<MeshAssetVertex>& vertices, std::span<uint32_t> indices)
{
    const meshopt_Stream streams[] = {
        {&vertices[0].position, sizeof(MeshAssetVertex::position), sizeof(MeshAssetVertex)},
        {&vertices[0].normal, sizeof(MeshAssetVertex::normal), sizeof(MeshAssetVertex)},
        {&vertices[0].uv, sizeof(MeshAssetVertex::uv), sizeof(MeshAssetVertex)},
    };

    std::vector<uint32_t> remap(vertices.size());
    const size_t merged_vertex_count = meshopt_generateVertexRemapMulti(
        remap.data(), indices.data(), indices.size(), vertices.size(), streams, std::size(streams));

    if (merged_vertex_count == vertices.size())
        return;

    std::vector<MeshAssetVertex> merged(merged_vertex_count);
    for (MeshAssetVertex& vertex : merged)
        vertex.tangent = glm::vec4{0.0f};

    for (size_t vertex = 0; vertex < vertices.size(); ++vertex)
    {
        const uint32_t mapped = remap[vertex];
        if (mapped == ~0u)
            continue;

        MeshAssetVertex& merged_vertex = merged[mapped];
        merged_vertex.position = vertices[vertex].position;
        merged_vertex.normal = vertices[vertex].normal;
        merged_vertex.uv = vertices[vertex].uv;
        merged_vertex.tangent += vertices[vertex].tangent;
    }

    for (MeshAssetVertex& vertex : merged)
    {
        const glm::vec3 tangent =
            glm::vec3(vertex.tangent) - vertex.normal * glm::dot(vertex.normal, glm::vec3(vertex.tangent));
        const float tangent_length = glm::length(tangent);
        const float handedness = vertex.tangent.w < 0.0f ? -1.0f : 1.0f;

        vertex.tangent = tangent_length > glm::epsilon<float>() ? glm::vec4(tangent / tangent_length, handedness)
                                                                : glm::vec4(0.0f, 0.0f, 0.0f, handedness);
    }

    meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
    vertices = std::move(merged);
}

LodIndexRanges generate_lods(std::span<const MeshAssetVertex> vertices, std::vector<uint32_t>& indices)
{
    const uint32_t lod0_index_count = static_cast<uint32_t>(indices.size());

    meshopt_optimizeVertexCache(indices.data(), indices.data(), lod0_index_count, vertices.size());

    LodIndexRanges lod_ranges{};
    lod_ranges.push_back({
        .first_index = 0,
        .index_count = lod0_index_count,
        .error = 0.0f,
    });

    while (lod_ranges.size() < LODS_TO_GENERATE)
    {
        const LodIndexRange prev_range = lod_ranges[lod_ranges.size() - 1];

        const uint32_t target_triangle_count =
            static_cast<uint32_t>(static_cast<float>(prev_range.index_count / 3) * LOD_REDUCTION);
        const uint32_t target_index_count = target_triangle_count * 3;

        if (target_index_count == 0)
            break;

        const size_t first_index = indices.size();
        indices.resize(first_index + lod0_index_count);

        float error = 0.0f;
        const size_t index_count = meshopt_simplifyWithAttributes(
            indices.data() + first_index,
            indices.data(),
            lod0_index_count,
            &vertices[0].position.x,
            vertices.size(),
            sizeof(MeshAssetVertex),
            &vertices[0].normal.x,
            sizeof(MeshAssetVertex),
            LOD_ATTRIBUTE_WEIGHTS,
            std::size(LOD_ATTRIBUTE_WEIGHTS),
            nullptr,
            target_index_count,
            1.0f,
            meshopt_SimplifyLockBorder,
            &error);

        // If meshoptimizer wasn't able to achieve at least MIN_ACHIEVED_REDUCTION of the requested reduction, the lod
        // is barely smaller than the previous one, so stop generating lods
        constexpr float MIN_ACHIEVED_REDUCTION = 0.5f;

        const uint32_t requested_reduction = prev_range.index_count - target_index_count;
        const uint32_t max_accepted_index_count =
            prev_range.index_count
            - static_cast<uint32_t>(static_cast<float>(requested_reduction) * MIN_ACHIEVED_REDUCTION);

        if (index_count == 0 || index_count > max_accepted_index_count)
        {
            indices.resize(first_index);
            break;
        }

        indices.resize(first_index + index_count);
        meshopt_optimizeVertexCache(
            indices.data() + first_index, indices.data() + first_index, index_count, vertices.size());

        const LodIndexRange range{
            .first_index = static_cast<uint32_t>(first_index),
            .index_count = static_cast<uint32_t>(index_count),
            .error = glm::max(error, prev_range.error),
        };

        if (lod_ranges.size() > 1 && range.error < prev_range.error * MIN_LOD_ERROR_GROWTH)
            lod_ranges[lod_ranges.size() - 1] = range;
        else
            lod_ranges.push_back(range);
    }

    return lod_ranges;
}

LodVertexRemap remap_vertices_by_lod(const LodIndexRanges& lod_ranges, std::span<uint32_t> indices, size_t vertex_count)
{
    LodVertexRemap result{};
    result.remap.resize(vertex_count, ~0u);

    uint32_t next_vertex = 0;

    for (uint8_t lod = static_cast<uint8_t>(lod_ranges.size()); lod-- > 0;)
    {
        const LodIndexRange& range = lod_ranges[lod];
        for (uint32_t i = range.first_index; i < range.first_index + range.index_count; ++i)
        {
            uint32_t& mapped = result.remap[indices[i]];
            if (mapped == ~0u)
            {
                mapped = next_vertex++;
                result.lod_vertex_counts[lod] += 1;
            }
        }
    }

    meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), result.remap.data());

    return result;
}

float lod_switch_screen_size(float relative_error)
{
    if (relative_error <= glm::epsilon<float>())
        return std::numeric_limits<float>::max();

    return LOD_MAX_SCREEN_ERROR / relative_error;
}

void fill_lod_metadata(
    MeshAssetMetadata& metadata,
    const LodIndexRanges& lod_ranges,
    const LodVertexRemap& vertex_remap,
    std::span<const MeshAssetVertex> vertices)
{
    const uint8_t num_lods = static_cast<uint8_t>(lod_ranges.size());

    const float error_scale = meshopt_simplifyScale(&vertices[0].position.x, vertices.size(), sizeof(MeshAssetVertex));
    const float bounding_radius = 0.5f * glm::length(metadata.bounding_box.max - metadata.bounding_box.min);
    const float relative_error_scale = error_scale / glm::max(bounding_radius, glm::epsilon<float>());

    for (uint8_t lod = 0; lod < num_lods; ++lod)
    {
        const float screen_size =
            lod + 1 < num_lods ? lod_switch_screen_size(lod_ranges[lod + 1].error * relative_error_scale) : 0.0f;

        metadata.lods.push_back(
            MeshAssetLod{
                .vertex_count = vertex_remap.lod_vertex_counts[lod],
                .index_count = lod_ranges[lod].index_count,
                .screen_size = screen_size,
            });
    }

    metadata.streamable_lods = glm::min<uint8_t>(NUM_STREAMABLE_LODS, num_lods - 1);
}

void write_lod_payload(
    std::span<uint8_t> payload,
    const MeshAssetMetadata& metadata,
    const LodIndexRanges& lod_ranges,
    const LodVertexRemap& vertex_remap,
    std::span<const MeshAssetVertex> vertices,
    std::span<const uint32_t> indices)
{
    memset(payload.data(), 0, metadata.get_total_size_bytes());

    std::array<uint32_t, MAX_NUM_MESH_LODS> lod_first_vertex{};
    std::array<uint64_t, MAX_NUM_MESH_LODS> lod_vertex_file_offset{};

    for (uint8_t lod = 0; lod < metadata.get_num_lods(); ++lod)
    {
        lod_first_vertex[lod] =
            static_cast<uint32_t>(metadata.get_lod_vertex_offset_bytes(lod) / metadata.get_vertex_stride_bytes());
        lod_vertex_file_offset[lod] = metadata.get_lod_vertex_file_offset(lod);

        memcpy(
            payload.data() + metadata.get_lod_index_file_offset(lod),
            indices.data() + lod_ranges[lod].first_index,
            metadata.get_lod_index_size_bytes(lod));
    }

    for (size_t vertex = 0; vertex < vertices.size(); ++vertex)
    {
        const uint32_t mapped = vertex_remap.remap[vertex];
        if (mapped == ~0u)
            continue;

        uint8_t lod = 0;
        while (mapped < lod_first_vertex[lod])
            ++lod;

        const uint64_t vertex_offset =
            lod_vertex_file_offset[lod]
            + static_cast<uint64_t>(mapped - lod_first_vertex[lod]) * metadata.get_vertex_stride_bytes();
        memcpy(payload.data() + vertex_offset, &vertices[vertex], sizeof(MeshAssetVertex));
    }
}

} // namespace

//
// MeshCooker
//

bool MeshCooker::should_cook(const CookRequest&, const TimestampDb&) const
{
    // Filtering done by importer
    return true;
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

    if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
    {
        context.reporter.error("Mesh '{}' has no vertices or faces, skipping", mesh->mName.C_Str());
        return;
    }

    const uint32_t lod0_index_count = mesh->mNumFaces * 3;

    std::vector<MeshAssetVertex> vertices(mesh->mNumVertices);

    std::vector<uint32_t> indices{};
    indices.reserve(LODS_TO_GENERATE > 1 ? lod0_index_count * LODS_TO_GENERATE / 2 : lod0_index_count);
    indices.resize(lod0_index_count);

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
            // TODO: Temporarily don't report error because can't really control error reasons with gltf-samples scenes.
            // context.reporter.error(
            //     "Mesh '{}' has a face with {} indices, expected 3", mesh->mName.C_Str(), face.mNumIndices);
            return;
        }

        for (uint32_t index_idx = 0; index_idx < face.mNumIndices; ++index_idx)
        {
            indices[face_idx * 3 + index_idx] = face.mIndices[index_idx];
        }
    }

    const glm::vec3 aabb_min = to_vec(mesh->mAABB.mMin);
    const glm::vec3 aabb_max = to_vec(mesh->mAABB.mMax);

    const float texel_factor = compute_texel_factor(vertices, indices);

    merge_duplicate_vertices(vertices, indices);

    const LodIndexRanges lod_ranges = generate_lods(vertices, indices);
    const LodVertexRemap vertex_remap = remap_vertices_by_lod(lod_ranges, indices, vertices.size());

    MeshAssetMetadata metadata{};
    metadata.index_format = IndexBufferFormat::UInt32;
    metadata.vertex_stride = sizeof(MeshAssetVertex);
    metadata.bounding_box = math::AABB{aabb_min, aabb_max};
    metadata.texel_factor = texel_factor;
    fill_lod_metadata(metadata, lod_ranges, vertex_remap, vertices);

    const uint64_t total_size = mesh_serialized_metadata_size(metadata) + metadata.get_total_size_bytes();

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Mesh");

    const uint64_t data_offset = mesh_serialize_metadata(metadata, data);
    write_lod_payload(data.subspan(data_offset), metadata, lod_ranges, vertex_remap, vertices, indices);

    const std::string filename = std::to_string(get_mesh_asset_id(request.virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });
}

} // namespace Mizu
