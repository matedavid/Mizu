#include "assimp_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "render/core/image_utils.h"
#include "render/material/material.h"
#include "render/model/mesh.h"
#include "render/renderer.h"
#include "render/shader/shader_manager.h"
#include "render/systems/sampler_state_cache.h"

#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/sampler_state.h"

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
    m_container_folder = m_path.parent_path();

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

    //
    // Load meshes
    //

    MIZU_ASSERT(scene->mNumMeshes > 0, "Model does not contain any mesh");

    m_meshes.reserve(scene->mNumMeshes);
    m_meshes_info.reserve(scene->mNumMeshes);
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
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

        Mesh::Description mesh_desc{};
        mesh_desc.name = mesh->mName.C_Str();

        m_meshes.push_back(std::make_shared<Mesh>(mesh_desc, vertices, indices));

        MeshInfo mesh_info{};
        mesh_info.mesh_idx = i;
        mesh_info.material_idx = mesh->mMaterialIndex;

        m_meshes_info.push_back(mesh_info);
    }

    //
    // Load materials
    //

    // Load textures
    std::unordered_map<std::string, std::shared_ptr<ImageResource>> texture_map;

    const auto get_texture_if_exists_else_add = [&](const std::string& name) {
        auto iter = texture_map.find(name);
        if (iter == texture_map.end())
        {
            const std::filesystem::path texture_path = m_container_folder / name;
            MIZU_ASSERT(
                std::filesystem::exists(texture_path),
                "Texture path: {} does not exist",
                texture_path.string().c_str());

            const auto texture = ImageUtils::create_texture2d(texture_path);
            iter = texture_map.insert({name, texture}).first;
        }

        return iter->second;
    };

    for (std::size_t i = 0; i < scene->mNumTextures; ++i)
    {
        const aiTexture* texture = scene->mTextures[i];
        get_texture_if_exists_else_add(texture->mFilename.C_Str());
    }

    ImageDescription default_desc{};
    default_desc.width = 1;
    default_desc.height = 1;
    default_desc.type = ImageType::Image2D;
    default_desc.format = ImageFormat::R8G8B8A8_SRGB;
    default_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    default_desc.name = "Default White";

    uint8_t white_data[] = {255, 255, 255, 255};
    const auto default_white_texture = ImageUtils::create_texture2d(default_desc, white_data);
    const auto default_white_texture_view = g_render_device->create_srv(default_white_texture);

    // Load materials
    const ShaderInstance& fragment_instance = PBROpaqueShaderFS{}.get_instance();

    MaterialShaderInstance material_shader{};
    material_shader.virtual_path = fragment_instance.virtual_path;
    material_shader.entry_point = fragment_instance.entry_point;

    m_materials.reserve(scene->mNumMeshes);
    for (std::size_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* mat = scene->mMaterials[i];

        const auto& material = std::make_shared<Material>(material_shader);

        // Albedo texture
        aiString albedo_path;
        if (mat->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &albedo_path) == aiReturn_SUCCESS)
        {
            const auto& albedo = get_texture_if_exists_else_add(albedo_path.C_Str());
            const auto albedo_view = g_render_device->create_srv(albedo);

            material->set_texture_srv("albedo", albedo_view);
        }
        else
        {
            material->set_texture_srv("albedo", default_white_texture_view);
        }

        // Metallic texture
        aiString metallic_path;
        if (mat->GetTexture(AI_MATKEY_METALLIC_TEXTURE, &metallic_path) == aiReturn_SUCCESS)
        {
            const auto& metallic = get_texture_if_exists_else_add(metallic_path.C_Str());
            const auto metallic_view = g_render_device->create_srv(metallic);

            material->set_texture_srv("metallic", metallic_view);
        }
        else
        {
            material->set_texture_srv("metallic", default_white_texture_view);
        }

        // Roughness texture
        aiString roughness_path;
        if (mat->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &roughness_path) == aiReturn_SUCCESS)
        {
            const auto& roughness = get_texture_if_exists_else_add(roughness_path.C_Str());
            const auto roughness_view = g_render_device->create_srv(roughness);

            material->set_texture_srv("roughness", roughness_view);
        }
        else
        {
            material->set_texture_srv("roughness", default_white_texture_view);
        }

        // AO texture
        aiString ao_path;
        if (mat->GetTexture(aiTextureType_LIGHTMAP, 0, &ao_path) == aiReturn_SUCCESS)
        {
            const auto& ao = get_texture_if_exists_else_add(ao_path.C_Str());
            const auto ao_view = g_render_device->create_srv(ao);

            material->set_texture_srv("ambientOcclusion", ao_view);
        }
        else
        {
            material->set_texture_srv("ambientOcclusion", default_white_texture_view);
        }

        //// Normal texture
        // aiString normal_path;
        // if (mat->GetTexture(aiTextureType_NORMALS, 0, &normal_path) == aiReturn_SUCCESS)
        //{
        //     const std::string name = std::string(normal_path.C_Str());
        //     auto& id = material->fetch<Phos::UUID>("uNormalMap");
        //     id = get_texture_if_exists_else_add(name);
        // }

        // Sampler
        material->set_sampler_state("sampler", get_sampler_state(SamplerStateDescription{}));

        [[maybe_unused]] const bool baked = material->bake();
        MIZU_ASSERT(baked, "Could not bake material");

        m_materials.push_back(material);
    }

    return true;
}

} // namespace Mizu
