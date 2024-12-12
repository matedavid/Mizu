#include "assimp_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "render_core/resources/mesh.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

std::optional<AssimpLoader> AssimpLoader::load(std::filesystem::path path)
{
    AssimpLoader loader;
    if (!loader.load_internal(std::move(path)))
    {
        return std::nullopt;
    }

    return loader;
}

bool AssimpLoader::load_internal(std::filesystem::path path)
{
    m_path = std::move(path);

    if (!std::filesystem::exists(m_path))
    {
        MIZU_LOG_ERROR("Could not open: {}", m_path.string());
        return false;
    }

    constexpr uint32_t import_flags =
        aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(m_path.string().c_str(), import_flags);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        MIZU_LOG_ERROR("Failed to Load with assimp: {}", m_path.string());
        return false;
    }

    MIZU_ASSERT(scene->mNumMeshes > 0, "Model does not contain any mesh");

    for (std::size_t i = 0; i < scene->mNumMeshes; ++i)
    {
        const auto mesh = scene->mMeshes[i];

        std::vector<Mesh::Vertex> vertices;
        std::vector<uint32_t> indices;

        // Vertices
        for (uint32_t j = 0; j < mesh->mNumVertices; ++j)
        {
            Mesh::Vertex vertex{};

            // Position
            const auto& vs = mesh->mVertices[j];
            vertex.position.x = vs.x;
            vertex.position.y = vs.y;
            vertex.position.z = vs.z;

            // Normals
            if (mesh->HasNormals())
            {
                const auto& ns = mesh->mNormals[j];
                vertex.normal.x = ns.x;
                vertex.normal.y = ns.y;
                vertex.normal.z = ns.z;
            }

            // Texture coordinates
            if (mesh->HasTextureCoords(0))
            {
                const auto& tc = mesh->mTextureCoords[0][j];
                vertex.uv.x = tc.x;
                vertex.uv.y = 1.0f - tc.y;
            }

            // Tangents
            // if (mesh->HasTangentsAndBitangents()) {
            //    const auto ts = mesh->mTangents[j];
            //    vertex.tangent.x = ts.x;
            //    vertex.tangent.y = ts.y;
            //    vertex.tangent.z = ts.z;
            // }

            vertices.push_back(vertex);
        }

        // Indices
        for (uint32_t j = 0; j < mesh->mNumFaces; ++j)
        {
            const aiFace& face = mesh->mFaces[j];

            for (uint32_t idx = 0; idx < face.mNumIndices; ++idx)
            {
                indices.push_back(face.mIndices[idx]);
            }
        }

        m_meshes.push_back(std::make_shared<Mesh>(vertices, indices));
    }

    return true;
}

} // namespace Mizu
