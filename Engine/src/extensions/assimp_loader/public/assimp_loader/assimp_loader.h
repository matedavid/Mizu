#pragma once

#include <filesystem>

#include "mizu_assimp_loader_module.h"

namespace Mizu
{

class MIZU_ASSIMP_LOADER_API AssimpLoader
{
  public:
    static uint32_t get_num_meshes(const std::filesystem::path& path);

  private:
    AssimpLoader() = default;
};

} // namespace Mizu