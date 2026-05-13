#pragma once

#include <cstddef>
#include <limits>
#include <string_view>

namespace Mizu
{

enum class AssetType
{
    Mesh,
    Texture,
};

template <typename Tag>
struct AssetHandle
{
  public:
    static constexpr size_t InvalidValue = std::numeric_limits<size_t>::max();

    AssetHandle() : m_id(InvalidValue) {}
    AssetHandle(size_t id) : m_id(id) {}

    size_t get_id() const { return m_id; }
    bool is_valid() const { return m_id != InvalidValue; }

    bool operator==(const AssetHandle&) const = default;

  private:
    size_t m_id;
};

using MeshAssetHandle = AssetHandle<struct MeshAssetTag>;
using TextureAssetHandle = AssetHandle<struct TextureAssetTag>;

[[maybe_unused]] inline std::string_view asset_type_to_string(AssetType type)
{
    switch (type)
    {
    case AssetType::Mesh:
        return "Mesh";
    case AssetType::Texture:
        return "Texture";
    }
}

} // namespace Mizu
