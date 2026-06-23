#include "assimp_loader/assimp_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "base/debug/logging.h"

namespace Mizu
{

uint32_t AssimpLoader::get_num_meshes(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        MIZU_LOG_ERROR("Could not open: {}", path.string());
        return false;
    }

    constexpr uint32_t import_flags =
        aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string().c_str(), import_flags);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        MIZU_LOG_ERROR("Failed to Load with assimp: {}", path.string());
        return false;
    }

    return scene->mNumMeshes;
}

} // namespace Mizu
