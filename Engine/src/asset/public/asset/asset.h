#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

namespace Mizu
{

struct AssetMount
{
    std::filesystem::path path;
    std::string name;
};

constexpr size_t MAX_NUM_ASSET_MOUNTS = 5;

class AssetMountTable
{
  public:
    void add_asset_mount(AssetMount mount)
    {
        if (const AssetMount* m = get_asset_mount_opt(mount.name))
        {
            MIZU_LOG_WARNING("Asset mount with name {} already exists", mount.name);
            return;
        }

        m_asset_mounts.push_back(mount);
    }

    AssetMount get_asset_mount(std::string_view name)
    {
        const AssetMount* mount = get_asset_mount_opt(name);
        if (mount == nullptr)
        {
            MIZU_ASSERT(false, "Asset mount with name {} does not exist", name);
            return {};
        }

        return *mount;
    }

    std::span<const AssetMount> get_asset_mounts() const { return m_asset_mounts; }

  private:
    inplace_vector<AssetMount, MAX_NUM_ASSET_MOUNTS> m_asset_mounts{};

    AssetMount* get_asset_mount_opt(std::string_view name)
    {
        for (AssetMount& mount : m_asset_mounts)
        {
            if (mount.name == name)
                return &mount;
        }

        return nullptr;
    }
};

struct MeshAssetVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

} // namespace Mizu