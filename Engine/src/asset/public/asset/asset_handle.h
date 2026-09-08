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
    Material,
    Prefab,
    ShaderDeclaration,
};

using AssetHandleId = uint64_t;

template <typename Tag>
struct AssetHandle
{
  public:
    static constexpr uint64_t InvalidValue = std::numeric_limits<AssetHandleId>::max();

    constexpr AssetHandle() : m_id(InvalidValue) {}
    constexpr AssetHandle(AssetHandleId id) : m_id(id) {}

    constexpr AssetHandleId get_id() const { return m_id; }
    constexpr bool is_valid() const { return m_id != InvalidValue; }

    constexpr bool operator==(const AssetHandle&) const = default;

  private:
    AssetHandleId m_id;
};

template <typename T>
struct is_asset_handle : std::false_type
{
};

template <typename Tag>
struct is_asset_handle<AssetHandle<Tag>> : std::true_type
{
};

template <typename T>
constexpr bool is_asset_handle_v = is_asset_handle<T>::value;

#define MIZU_CREATE_ASSET_HANDLE_TYPE(HandleTypeName)               \
    using HandleTypeName = AssetHandle<struct HandleTypeName##Tag>; \
    static_assert(is_asset_handle_v<MeshAssetHandle>, #HandleTypeName " should satisfy IsAssetHandleType")

MIZU_CREATE_ASSET_HANDLE_TYPE(MeshAssetHandle);
MIZU_CREATE_ASSET_HANDLE_TYPE(TextureAssetHandle);
MIZU_CREATE_ASSET_HANDLE_TYPE(MaterialAssetHandle);
MIZU_CREATE_ASSET_HANDLE_TYPE(PrefabAssetHandle);
MIZU_CREATE_ASSET_HANDLE_TYPE(ShaderDeclarationAssetHandle);

#undef MIZU_CREATE_ASSET_HANDLE_TYPE

} // namespace Mizu

template <typename Tag>
struct std::hash<Mizu::AssetHandle<Tag>>
{
    size_t operator()(const Mizu::AssetHandle<Tag>& handle) const { return handle.get_id(); }
};
