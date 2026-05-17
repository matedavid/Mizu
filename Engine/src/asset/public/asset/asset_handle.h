#pragma once

#include <cstddef>
#include <cstdint>
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
    static constexpr uint64_t InvalidValue = std::numeric_limits<uint64_t>::max();

    AssetHandle() : m_id(InvalidValue) {}
    AssetHandle(uint64_t id) : m_id(id) {}

    uint64_t get_id() const { return m_id; }
    bool is_valid() const { return m_id != InvalidValue; }

    bool operator==(const AssetHandle&) const = default;

  private:
    uint64_t m_id;
};

using MeshAssetHandle = AssetHandle<struct MeshAssetTag>;
using TextureAssetHandle = AssetHandle<struct TextureAssetTag>;

template <typename T>
struct is_asset_handle : std::false_type
{
};

template <typename Tag>
struct is_asset_handle<AssetHandle<Tag>> : std::true_type
{
};

template <typename T>
concept IsAssetHandleType = is_asset_handle<T>::value;

static_assert(IsAssetHandleType<MeshAssetHandle>, "MeshAssetHandle should satisfy IsAssetHandleType");
static_assert(IsAssetHandleType<TextureAssetHandle>, "TextureAssetHandle should satisfy IsAssetHandleType");

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

template <typename Tag>
struct std::hash<Mizu::AssetHandle<Tag>>
{
    size_t operator()(const Mizu::AssetHandle<Tag>& handle) const { return handle.get_id(); }
};
